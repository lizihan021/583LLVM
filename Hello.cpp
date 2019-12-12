//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
// #include "llvm/IR/LLVMContext.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasSetTracker.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
// #include "llvm/Analysis/MemorySSA.h"
// #include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <algorithm>
#include <utility>
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
using namespace llvm;

#define DEBUG_TYPE "hello"

// static LLVMContext TheContext;
// static IRBuilder<> Builder(TheContext);
// static std::unique_ptr<Module> TheModule;

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  bool checkDepAndSetInFB(BranchInst* ifInst, BranchProbabilityInfo *BPI, DependenceInfo *DI, BasicBlock*& infreqBlock, BasicBlock*& freqBlock){
      if (ifInst == nullptr) return false;

      freqBlock = ifInst->getSuccessor(1);
      infreqBlock = ifInst->getSuccessor(0);

      // BranchProbability bp =
      //   BPI->getEdgeProbability(ifInst->getParent(), infreqBlock);

      // if (bp.getNumerator() >= .8 * bp.getDenominator()) {
      //   std::swap(freqBlock, infreqBlock);
      // }

      for (auto II = freqBlock->begin(); II != freqBlock->end(); ++ II) {
        Instruction &Dst = *II;
        for (auto infII = infreqBlock->begin(); infII != infreqBlock->end(); ++ infII) {
          Instruction &Src = * infII;
          Dependence *dep = DI->depends(&Src, &Dst, false).get();
          if (dep != NULL) {
            errs() << "Dep. detected, apply our pass\n";
            // if (dep->isConfused()) errs() << "[C] ";
            dep->getDst()->print(errs());
            errs() << "\n";
            // errs() << "   ---> ";
            dep->getSrc()->print(errs(), false);
            errs() << "\n";
            return true;
          }
        }
      }
      return false;
  }

  struct Hello : public LoopPass {
    static char ID; // Pass identification, replacement for typeid
    Hello() : LoopPass(ID) {}


    bool runOnLoop(Loop *L, LPPassManager &LPM) override {
      BranchProbabilityInfo *BPI = &getAnalysis<BranchProbabilityInfoWrapperPass>().getBPI();
      BlockFrequencyInfo *BFI = &getAnalysis<BlockFrequencyInfoWrapperPass>().getBFI();
      DependenceInfo *DI = &getAnalysis<DependenceAnalysisWrapperPass>().getDI();
      LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
      DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();

      // MemorySSA *MSSA = &getAnalysis<MemorySSAWrapperPass>().getMSSA();
      // std::unique_ptr<MemorySSAUpdater> MSSAU = make_unique<MemorySSAUpdater>(MSSA);
      // if (L->getParent()->getName() != "main")
      //   return false;

      errs() << "================================" << "\n";
      errs() << "===      start our pass      ===" << "\n";
      errs() << "================================" << "\n";

      BasicBlock* loopHeader = L->getLoopPreheader()->getUniqueSuccessor();
      DetachInst* detachInst = dyn_cast<DetachInst>(loopHeader->getTerminator());
      if (detachInst == nullptr) return false;
      BasicBlock* loopIfBlock = detachInst->getDetached();
      BasicBlock* loopLatch = detachInst->getContinue();
      BranchInst* ifInst = dyn_cast<BranchInst>(loopIfBlock->getTerminator());

      // get infreqBlock and freqBlock
      BasicBlock* infreqBlock;
      BasicBlock* freqBlock;
      if (!checkDepAndSetInFB(ifInst, BPI, DI, infreqBlock, freqBlock)) {
        errs() << "infre" << "\n";
        return false;
      }
      errs() << "infreqBlockName " << infreqBlock->getName() << "\n";
      errs() << "freqBlockName " << freqBlock->getName() << "\n";

      Function* F = ifInst->getFunction();
      BasicBlock* loopReattachBlock = freqBlock->getUniqueSuccessor();

      // clone if block
      ValueToValueMapTy VMap;
      BasicBlock *ifBlockClone = CloneBasicBlock(loopIfBlock, VMap, ".clone", F);
      SmallVector<BasicBlock *, 1> blockList = {ifBlockClone};
      remapInstructionsInBlocks(blockList, VMap);

      // get cloned if instruction and infrequent branch instruction
      BranchInst* ifInstClone = dyn_cast<BranchInst>(ifBlockClone->getTerminator());
      BranchInst* infreqBr = dyn_cast<BranchInst>(infreqBlock->getTerminator());
      ReattachInst* reattachInst = dyn_cast<ReattachInst>(loopReattachBlock->getTerminator());

      // change true branch of ifBlock and false branch of ifBlockClone
      ifInst->setSuccessor(0, loopReattachBlock);
      ifInstClone->setSuccessor(1, loopLatch);

      // change infreqBlock to point to latch
      infreqBr->setSuccessor(0, loopLatch);

      // change reattach and continue to cloned block
      detachInst->setSuccessor(1, ifBlockClone);
      reattachInst->setSuccessor(0, ifBlockClone);

      // add cloned BB to loop
      L->addBasicBlockToLoop(ifBlockClone, *LI);

      // create sync before infrequent path
      BasicBlock* newSyncBlock = SplitBlock(infreqBlock, &(*infreqBlock->begin()), DT, LI);
      std::swap(newSyncBlock, infreqBlock);

      BasicBlock* exitBlock = L->getUniqueExitBlock();

      Instruction* syncInstClone = (*exitBlock->begin()).clone();
      syncInstClone->insertBefore(newSyncBlock->getTerminator());
      syncInstClone->setSuccessor(0, infreqBlock);
      newSyncBlock->getTerminator()->eraseFromParent();

      errs() << "### log end\n";

      return true;

    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<BranchProbabilityInfoWrapperPass>();
      AU.addRequired<BlockFrequencyInfoWrapperPass>();
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addRequired<DependenceAnalysisWrapperPass>();
      AU.addRequired<DominatorTreeWrapperPass>();

      // AU.addRequired<MemorySSAWrapperPass>();
      // AU.addPreserved<MemorySSAWrapperPass>();
      getLoopAnalysisUsage(AU);
    }
  };
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");

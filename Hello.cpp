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
#include <algorithm>
#include <utility>
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/CFG.h"
using namespace llvm;

#define DEBUG_TYPE "hello"

// static LLVMContext TheContext;
// static IRBuilder<> Builder(TheContext);
// static std::unique_ptr<Module> TheModule;

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
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

      auto exitBlock = L->getUniqueExitBlock();
      if (exitBlock == nullptr) {
        return false;
      }

      auto exitingBlock = *pred_begin(exitBlock);
      errs() << "jfdkslajdlksajflkdsa\n";

      auto exitingBranchInst = exitingBlock->end();

      exitingBranchInst--;
      exitingBranchInst--;
      exitingBranchInst--;

      Instruction *I = &(*exitingBranchInst);
      I->print(errs());
      auto module = I->getFunction()->getParent();

      auto newExitingBlock = SplitBlock(exitingBlock, I, DT, LI);
      auto threadBlock = SplitBlock(exitingBlock, &(*exitingBlock->begin()), DT, LI);
      auto detachBlock = exitingBlock;

      exitBlock = L->getUniqueExitBlock();
      if (exitBlock == nullptr) {
        return false;
      }
      auto newExitBlock = SplitBlock(exitBlock, &(*exitBlock->begin()), DT, LI);
      
      auto preHeader = L->getLoopPreheader();
      IRBuilder<> TmpB(preHeader, preHeader->begin());
      auto SyncRegion = TmpB.CreateCall(
        Intrinsic::getDeclaration(module, Intrinsic::syncregion_start), {});

      // Create the detach.
      IRBuilder<> Builder(detachBlock->getTerminator());
      DetachInst *DetachI = Builder.CreateDetach(threadBlock, newExitingBlock, dyn_cast<Instruction>(SyncRegion));
      detachBlock->getTerminator()->eraseFromParent();
      // Create the reattach.
      Builder.SetInsertPoint(threadBlock->getTerminator());
      ReattachInst *ReattachI = Builder.CreateReattach(newExitingBlock, SyncRegion);
      threadBlock->getTerminator()->eraseFromParent();
      // Create the sync.
      Builder.SetInsertPoint(exitBlock->getTerminator());
      SyncInst *SyncI = Builder.CreateSync(newExitBlock, SyncRegion);
      exitBlock->getTerminator()->eraseFromParent();

      errs() << "### end\n";
      return true;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<BranchProbabilityInfoWrapperPass>();
      AU.addRequired<BlockFrequencyInfoWrapperPass>();
      AU.addPreserved<LoopInfoWrapperPass>();
      AU.addRequired<DependenceAnalysisWrapperPass>();
      AU.addPreserved<DominatorTreeWrapperPass>();

      // AU.addRequired<MemorySSAWrapperPass>();
      // AU.addPreserved<MemorySSAWrapperPass>();
      getLoopAnalysisUsage(AU);
    }
  };
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");


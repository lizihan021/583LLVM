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

      BasicBlock* preHeader = L->getLoopPreheader();
      BasicBlock* pfor_detach = preHeader->getUniqueSuccessor();
      DetachInst* detachInst = dyn_cast<DetachInst>(pfor_detach->getTerminator());
      BasicBlock* pfor_body = detachInst->getDetached();
      BasicBlock* pfor_inc = detachInst->getContinue();

      BranchInst* ifInst = dyn_cast<BranchInst>(pfor_body->getTerminator());
      Function* F = ifInst->getFunction();
      BasicBlock* infreqBlock = ifInst->getSuccessor(0);
      errs() << "infreqBlockName " << infreqBlock->getName() << "\n";
      BasicBlock* freqBlock = ifInst->getSuccessor(1);
      errs() << "freqBlockName " << freqBlock->getName() << "\n";

      BasicBlock* if_end = freqBlock->getUniqueSuccessor();
      ReattachInst* reattachInst = dyn_cast<ReattachInst>(if_end->getTerminator());

      // ValueToValueMapTy VMap;
      // BasicBlock *pfor_body_clone = CloneBasicBlock(pfor_body, VMap, "pfor.body.clone", F);
      BasicBlock* pfor_body_clone = SplitBlock(pfor_inc, &(*pfor_inc->begin()), DT, LI);
      std::swap(pfor_body_clone, pfor_inc);

      for (auto II = pfor_body->begin(), IE = pfor_body->end(); II != IE; ++II) {
        Instruction* II_clone = (*II).clone();
        II_clone->insertBefore(pfor_body_clone->getTerminator());
        II_clone->print(errs());
      }

      pfor_body_clone->getTerminator()->eraseFromParent();
      BranchInst* ifInstClone = dyn_cast<BranchInst>(pfor_body_clone->getTerminator());
      ICmpInst* ifConditionClone = dyn_cast<ICmpInst>(pfor_body_clone->begin());
      ifInstClone->setSuccessor(0, infreqBlock);
      ifInstClone->setSuccessor(1, pfor_inc);
      ifInstClone->setCondition(ifConditionClone);

      BasicBlock* if_end_split = SplitBlock(if_end, &(*if_end->getTerminator()), DT, LI);

      // Todo, check which one is the frequent block.
      PHINode* phi_node = dyn_cast<PHINode>(if_end->begin());
      phi_node->print(errs());
      BasicBlock* incommingBlock = phi_node->getIncomingBlock(0);
      Value* frequentValue;
      if (incommingBlock == freqBlock) {
        frequentValue = phi_node->getIncomingValue(0);
      } else {
        frequentValue = phi_node->getIncomingValue(1);
      }

      // TODO: detect all usage
      StoreInst* store_inst = dyn_cast<StoreInst>(--(--if_end->end()));
      store_inst->setOperand(0, frequentValue);

      if_end->begin()->eraseFromParent();

      BranchInst* ifThenBr = dyn_cast<BranchInst>(infreqBlock->getTerminator());
      ifThenBr->setSuccessor(0, pfor_inc);

      BranchInst* infreqBr = dyn_cast<BranchInst>(pfor_body->getTerminator());
      infreqBr->setSuccessor(0, if_end_split);

      ifInst->setSuccessor(0, freqBlock);







      // pfor_body_clone->getTerminator()->eraseFromParent();

      // BranchInst* ifInstClone = dyn_cast<BranchInst>(pfor_body_clone->getTerminator());
      // ifInstClone->setSuccessor(1, pfor_inc);

      // detachInst->setSuccessor(1, pfor_body_clone);
      // reattachInst->setSuccessor(0, pfor_body_clone);
      // //
      // //
      // ifInst->setSuccessor(0, if_end);
      // //
      // BranchInst* infreqBr = dyn_cast<BranchInst>(infreqBlock->getTerminator());
      // infreqBr->setSuccessor(0, pfor_inc);
      // //
      // ifInstClone->setSuccessor(0, infreqBlock);
      //

      // (--(--if_end->end()))->eraseFromParent();
      // (--(--if_end->end()))->eraseFromParent();
      // (--(--if_end->end()))->eraseFromParent();




      errs() << "### log end ###\n";

      return true;

      // auto exitBlock = L->getUniqueExitBlock();
      // if (exitBlock == nullptr) {
      //   return false;
      // }
      // // errs() << exitBlock->getParent()->getName() << "\n";
      // // if (exitBlock->getParent()->getName() != "main") {
      // //   return false;
      // // };
      // // errs() << exitBlock->getParent()->getName() << "\n";


      // auto exitingBlock = *pred_begin(exitBlock);
      // errs() << "jfdkslajdlksajflkdsa\n";

      // auto exitingBranchInst = exitingBlock->end();

      // exitingBranchInst--;
      // exitingBranchInst--;
      // exitingBranchInst--;

      // Instruction *I = &(*exitingBranchInst);
      // I->print(errs());
      // auto module = I->getFunction()->getParent();

      // auto newExitingBlock = SplitBlock(exitingBlock, I, DT, LI);
      // auto threadBlock = SplitBlock(exitingBlock, &(*exitingBlock->begin()), DT, LI);
      // auto detachBlock = exitingBlock;

      // exitBlock = L->getUniqueExitBlock();
      // if (exitBlock == nullptr) {
      //   return false;
      // }
      // auto newExitBlock = SplitBlock(exitBlock, &(*exitBlock->begin()), DT, LI);

      // auto preHeader = L->getLoopPreheader();
      // IRBuilder<> TmpB(preHeader, preHeader->begin());
      // auto SyncRegion = TmpB.CreateCall(
      //   Intrinsic::getDeclaration(module, Intrinsic::syncregion_start), {});

      // // Create the detach.
      // IRBuilder<> Builder(detachBlock->getTerminator());
      // DetachInst *DetachI = Builder.CreateDetach(threadBlock, newExitingBlock, dyn_cast<Instruction>(SyncRegion));
      // detachBlock->getTerminator()->eraseFromParent();
      // // Create the reattach.
      // Builder.SetInsertPoint(threadBlock->getTerminator());
      // ReattachInst *ReattachI = Builder.CreateReattach(newExitingBlock, SyncRegion);
      // threadBlock->getTerminator()->eraseFromParent();
      // // Create the sync.
      // Builder.SetInsertPoint(exitBlock->getTerminator());
      // SyncInst *SyncI = Builder.CreateSync(newExitBlock, SyncRegion);
      // exitBlock->getTerminator()->eraseFromParent();

      // // Sync infrequent pass
      // BasicBlock* header = preHeader->getSingleSuccessor();
      // BranchInst* headerBR = dyn_cast<BranchInst>(header->getTerminator());
      // headerBR->print(errs());
      // BasicBlock* IFPBB = headerBR->getSuccessor(0) == detachBlock ?
      //   headerBR->getSuccessor(1) : headerBR->getSuccessor(0);
      // BasicBlock* IFPBBOld =  SplitBlock(IFPBB, &*IFPBB->begin(), DT, LI);

      // Builder.SetInsertPoint(IFPBB->getTerminator());
      // SyncInst *SyncIFP = Builder.CreateSync(IFPBBOld, SyncRegion);
      // IFPBB->getTerminator()->eraseFromParent();

      // errs() << "### end\n";
      // return true;
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

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
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasSetTracker.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include <algorithm>
#include <utility>
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"

using namespace llvm;

#define DEBUG_TYPE "hello"


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
      LoopAccessInfo* LAI = &getAnalysis<LoopAccessAnalysis>().getResult();
      const MemoryDepChecker& MDC = LAI.getDepChecker();
      const SmallVectorImpl< Dependence > * A = MDC.getDependences();
      for (auto PI = A->begin(); PI != A->end(); ++ PI) {
          PI->getDst()->print(errs(), false);
          errs() << "    ----->";
          PI->getSrc()->print(errs(), false);
          errs() << "\n";
      }
      errs() << "### Hello: \n";

      for (auto BB1 = L->block_begin(); BB1 != L->block_end(); ++BB1) {
        for (auto II = (*BB1)->begin(), IE = (*BB1)->end(); II != IE; II++) {
           Instruction &Dst = *II;
           for (auto BB2 = L->block_begin(); BB2 != L->block_end(); ++BB2) {
              for (auto JI = (*BB2)->begin(), JE = (*BB2)->end(); JI != JE; JI++) {
                 Instruction &Src = *JI;
                 Dependence *dep;
                 auto infoPtr = DI->depends(&Src, &Dst, false); /* depends(&Src, &Dest, true); */
                 dep = infoPtr.get();
                 if (dep != NULL) {
                   if (dep->isConfused()) errs() << "[C] ";
                   dep->getDst()->print(errs(), false);
                   errs() << "   ---> ";
                   dep->getSrc()->print(errs(), false);
                   errs() << "\n";
                 }
              }
           }
        }
      }

      errs() << "### end\n";
      return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<BranchProbabilityInfoWrapperPass>();
      AU.addRequired<BlockFrequencyInfoWrapperPass>();
      AU.addPreserved<LoopInfoWrapperPass>();
      AU.addRequired<DependenceAnalysisWrapperPass>();
      AU.getResult<LoopAccessAnalysis>();
      getLoopAnalysisUsage(AU);
    }
  };
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");


#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Constants.h"

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <utility>

using namespace llvm;

/*
TODO: address potential issue that our profile data might be invalidated by other transforming passes
running BEFORE our last pass.
*/

/*
    auto *SE = getAnalysisIfAvailable<ScalarEvolutionWrapperPass>();
    MemorySSA *MSSA = EnableMSSALoopDependency
                          ? (&getAnalysis<MemorySSAWrapperPass>().getMSSA())
                          : nullptr;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<BranchProbabilityInfoWrapperPass>();
    AU.addRequired<BlockFrequencyInfoWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addRequired<DominanceFrontierWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    if (EnableMSSALoopDependency) {
      AU.addRequired<MemorySSAWrapperPass>();
      AU.addPreserved<MemorySSAWrapperPass>();
    }
    AU.addRequired<TargetTransformInfoWrapperPass>();
    getLoopAnalysisUsage(AU);
  }
*/
/*

*/

namespace {
typedef std::pair<MemoryLocation, MemoryLocation> MemLocPair;

struct hashMemLocPair { 
  size_t operator()(const MemLocPair& p) const {
    auto hash1 = std::hash<void*>{}((void*)p.first.Ptr); 
    auto hash2 = std::hash<void*>{}((void*)p.second.Ptr); 
    return hash1 ^ hash2; 
  } 
};

struct AliasStats {
  uint32_t num_collisions;
  uint32_t num_comparisons;

  AliasStats() : num_collisions(0), num_comparisons(0) {}
};

struct InstLogAnalysis {
  inline static std::unordered_map<MemLocPair, AliasStats, hashMemLocPair> memLocPairToAliasStats = {};

  static double getAliasProbability(const MemoryLocation& loc_a, const MemoryLocation& loc_b) {
    if (loc_a.Ptr == loc_b.Ptr) {
      return 1.0;
    }
    
    auto it = InstLogAnalysis::memLocPairToAliasStats.find({loc_a, loc_b});
    if (it == InstLogAnalysis::memLocPairToAliasStats.end()) {
      return 0.0; // TOCHECK
    }
    return (double)it->second.num_collisions / it->second.num_comparisons;
  }
};

struct InstLogAnalysisWrapperPass : public ModulePass {
  static char ID;
  std::unordered_map<size_t, MemoryLocation> idToMemLoc;

  InstLogAnalysisWrapperPass() : ModulePass(ID) {}

  std::unordered_map<size_t, MemoryLocation> getIdToMemLocMapping(Module &m, Function* instLogFunc) const {
    std::unordered_map<size_t, MemoryLocation> ret;
    std::unordered_set<const Value*> visitedPtrs;
    size_t currId = 0;

    for (auto& func : m) {
      if (&func == instLogFunc) continue;
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
            auto memLoc = MemoryLocation::get(&inst);
            if (!visitedPtrs.count(memLoc.Ptr)) {
              ret[currId++] = memLoc;
              visitedPtrs.insert(memLoc.Ptr);
              errs() << "new id: " << currId << '\n';
            }
            else {
              errs() << "already mapped\n";
            }
          }
        }
      }
    }
    return ret;
  }

  std::unordered_map<MemLocPair, AliasStats, hashMemLocPair> parseLogAndGetAliasStats() const {
    std::unordered_map<size_t, uint64_t> idToShadowValue;
    std::unordered_map<MemLocPair, AliasStats, hashMemLocPair> memLocPairToAliasStats;

    size_t instIdIn;
    void* memAddrIn_void; // TODO: change to uint64_t directly?
    std::ifstream ins("../583simple/log.log");
    while (ins >> instIdIn >> memAddrIn_void) {
      auto memLocIn = idToMemLoc.at(instIdIn);
      uint64_t memAddrIn = (uint64_t)memAddrIn_void;
      idToShadowValue[instIdIn] = memAddrIn;

      /* TOCHECK: only compare against Memlocs that currently have a shadow value */
      for (auto it_shadow = idToShadowValue.begin(); it_shadow != idToShadowValue.end(); ++it_shadow) {
        auto memLocCompare = idToMemLoc.at(it_shadow->first);
        uint64_t memAddrCompare = it_shadow->second;

        if (memLocCompare.Ptr != memLocIn.Ptr) { // don't compute aliasing stats with itself
          auto& pairAliasStats = memLocPairToAliasStats[{memLocIn, memLocCompare}];
          pairAliasStats.num_comparisons++;
          if (memAddrIn == memAddrCompare) {
            pairAliasStats.num_collisions++;
            errs() << "\tCOLLISION DETECTED\n";
          }
        }
      }
    }

    return memLocPairToAliasStats;
  }

  void testGetAliasProba(Module& m, Function* instLogFunc, size_t targetId_a, size_t targetId_b) {
    MemoryLocation memLoc_a, memLoc_b;
    std::unordered_set<const Value*> visitedPtrs;
    size_t currId = 0;

    for (auto& func : m) {
      if (&func == instLogFunc) continue;
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
            auto memLoc = MemoryLocation::get(&inst);
            if (!visitedPtrs.count(memLoc.Ptr)) {
              if (currId == targetId_a) memLoc_a = memLoc;
              else if (currId == targetId_b) memLoc_b = memLoc;
              ++currId;
              visitedPtrs.insert(memLoc.Ptr);
            }
          }
        }
      }
    }


    double aliasProba = InstLogAnalysis::getAliasProbability(memLoc_a, memLoc_b);
    errs() << "AliasProba between ID " << targetId_a << " and ID " << targetId_b << " is " << aliasProba << '\n';
  }

  bool runOnModule(Module &m) override {
    auto* instLogFunc = m.getFunction("_inst_log");
    assert(instLogFunc && "instLogFunc not found");

    idToMemLoc = getIdToMemLocMapping(m, instLogFunc);
    errs() << "********\nbuilding map done\n\n";

    std::unordered_map<MemLocPair, AliasStats, hashMemLocPair> memLocPairToAliasStats = parseLogAndGetAliasStats();
    errs() << "********\nparsing done \n\n";

    InstLogAnalysis::memLocPairToAliasStats = memLocPairToAliasStats;

    testGetAliasProba(m, instLogFunc, 2, 5);
    testGetAliasProba(m, instLogFunc, 10, 8);
    testGetAliasProba(m, instLogFunc, 1, 1);
    return false;
  }

// private:
  // InstLogAnalysis instLogAnalysis;

}; // end of struct InstLogAnalysisWrapperPass
}  // end of anonymous namespace

char InstLogAnalysisWrapperPass::ID = 0;
static RegisterPass<InstLogAnalysisWrapperPass> X("fp_analysis", "InstLogAnalysisWrapperPass Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);


 /*
issues:
  might detect "fake"/"useless?" aliasing across iterations/function calls
  one fix would be to change interface to doAlias(ptrA, ptrB, %i) where i is where "start looking";

loop:
    %1: load(ptrA);      ptrA==0x1
    %2: load(ptrB);      ptrB==0x2
loop:
    %1: load(ptrA);      ptrA==0x2
    doAlias(ptrA, ptrB, i);
    %2: load(ptrB);      ptrB==0x3
loop:
    %1: load(ptrA);      ptrA==0x3
    %2: load(ptrB);      ptrB==0x4

    %3: ptrC = 0x1


ptrA = alloca
x = alloca
store(500, %x)
store(600, %x)
y = load %x

int y;
int Y;
int* yPtr = &y;
int* yPtr1 = &Y;
int** yPtrPtr = &yPtr;
*yPtrPtr = &yPtr1;



%y = alloca i32, align 4
(LOG: %y is 0xFFFFF...)
%yPtr = alloca i32*, align 8
(LOG: %yPtr is 0xAAAAA...)
store i32* %y, i32** %yPtr, align 8
%yPtr2 = alloca i32*, align 8

%yPtrPtr = alloca i32**, align 8
(LOG: %yPtrPtr is 0xBBBBB...)
store i32** yPtr2, i32*** yPtrPtr, align 8
%val = alloca ...
%temp = load i32* yPtr2
store temp, val

load(ptrA);      ptrA==0x3
load(ptrB);      ptrB==0x4
load(ptrB);      ptrB==0x5
load(ptrB);      ptrB==0x3
store(ptrB)

1st key is smallest ptr

fn:
    %1: load(ptrA);      ptrA==0x1
    %2: load(ptrB);      ptrB==0x2
    %3: store(ptrB);
fn:
    %1: load(ptrA);      ptrA==0x2
    %2: load(ptrB);      ptrA==0x3

  */
 /*
  ptrToVal
  valToPtr

  when ptr is assigned new value -> check if that value aliases with any other ptr

  what have we learned?
    memory location prob not unique to an inst?
    need to fix our stuff
 */

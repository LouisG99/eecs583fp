#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Constants.h"

#include "../ANALYSIS/analysispass.cpp"

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <fstream>
#include <utility>

using namespace llvm;

/*
loop:
  store(memLocB) (memLoc IS variant)
  %k = load(memLocA) (memLocA invariant)
  memLocA and memLocB never alias -> hoist load?
load(memLocA)\

common expression elimination

%memLocA = alloca i32 // I have storage somewhere for an i32 which could be a register


int main() {
  Class* a = new Class(argv[1]);
  Class* b = a;

  fn(a);
  fn(b);
}

common expression elimination?
double calcStuff(rect* first, rect* second) {
  return sqrt(first->area) / (sqrt(second->area) + 1);
}

arg1 = load f64 first
val1 = call sqrt(arg1)
arg2 = load f64 second
val2 = call sqrt(arg2)
val3 = add val2 1
val4 = div val1 val3
ret val4

...

arg1 = load f64 first
val1 = call sqrt(arg1)
val2 = val1
if (first != second) {
  arg2pre = load f64 second
  val2pre = call sqrt(arg2)
  val2 = val2pre -> store
}
arg2 = phi(arg1, arg2pre)
val2 = phi(val1, val2pre)
val3 = add val2 1
val4 = div val1 val3
ret val4


%i = load(memLocA)
%a = functionCall(%i) //
sqrt()
fib()
num_digits()
log()
toUpper()

void fcn(int og[], int copy[], int start, int end){
  memcpy(og, copy, len);
  if (og != copy) // do copy
}
int main(){
  fcn()
}

%j = load(memLocB)
%b = functionCall(%j)

%i = load(memLocA)
if (memLocA != memLocB) %k = load(memLocB)
%j = phi(%i, %k)


%hoisted = load(memLocA)
loop:
  %k1 = phi(%k2, %hoisted)
  store(memLocB)
  if (memLocB == memLocA) %i1 = load(memLocA)
  %k2 = phi(%i1, %k1)
  memLocA and memLocB never alias -> hoist load?

  for (i = 0 -> 100) {
    b[i] = x;
     a = b[CONSTANT] (not alias if CONSTANT > 100)
    a[i + 1] = y;
  }
*/


/*
PLAN:
  - pure function optimization
  - maybe also LICM (see if can disable register promotion)

Note: can examine compiled code with
https://stackoverflow.com/questions/10990018/how-to-generate-assembly-code-with-clang-in-intel-syntax
*/

namespace {

struct OptimOnAliasProfilePass : public ModulePass {
  static char ID;
  double aliasProbaThreshold = 0.80;

  OptimOnAliasProfilePass() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage& AU) const override {
    AU.addRequired<fp583::InstLogAnalysisWrapperPass>();
  }

  MemoryLocation getMemLocFromPtr(const Value* val) {
    return MemoryLocation(val); // TOCHECK: this is jank (this should work bc analysis just looks at ptr value but in practice it's bad style)
  }

  bool areFunctionCallsIdentical(const fp583::InstLogAnalysis& instLogAnalysis, CallBase* call1, CallBase* call2, std::vector<std::pair<Value*, Value*>>& ptrArgsVals){
    assert(call1->getCalledFunction() == call2->getCalledFunction());
    auto* calledF = call1->getCalledFunction();
    assert(calledF->getName().contains("_PURE_") && "function name contains PURE");

    for (unsigned int i = 0; i < calledF->arg_size(); ++i) {
      auto* arg = calledF->getArg(i);
      auto* val1 = call1->getArgOperand(i);
      auto* val2 = call2->getArgOperand(i);

      if (arg->getType()->isPointerTy()) { // TOCHECK: this should be how we check for pointers
        auto memLoc1 = getMemLocFromPtr(val1), memLoc2 = getMemLocFromPtr(val2);
        double probaAlias = instLogAnalysis.getAliasProbability(memLoc1, memLoc2);
        if (probaAlias < aliasProbaThreshold) {
          return false;
        }
        ptrArgsVals.push_back({val1, val2});
      }
      else if (val1 != val2) {
        return false;
      }
    }
    return true;
  }

  bool isFunctionPure(Function* f) {
    return f->getName().contains("_PURE_");
  }

  void removeFunctionCallAndFixUp(CallBase* currCall, CallBase* prevCall, const std::vector<std::pair<Value*, Value*>>& ptrArgsVals) {
    auto* currBB = currCall->getParent();
    // auto* followingBB = currBB->splitBasicBlock(currCall);
    auto* followingBB = currBB->splitBasicBlock(currCall->getNextNode()); // use ^ (this is necessary right now so it doesnt loop forever tho)

    Instruction* lastComp = nullptr;
    for (auto& [val1, val2] : ptrArgsVals) {
      auto* valComp = new ICmpInst(currBB->getTerminator(), ICmpInst::ICMP_NE, val1, val2);
      if (lastComp) {
        lastComp = BinaryOperator::CreateAnd(valComp, lastComp);
        lastComp->insertBefore(currBB->getTerminator());
      }
      else {
        lastComp = valComp;
      }
    }

    /** FIXUP
     * for ALL pointer arguments:
     *  check if they're NOT equal with if branch
     *  if so -> do function call and store results
     **/
  }

  /* example:
    // main()
    //  int val1 = 5;
    //  int val2 = 6;
    //   fn(val1, ptrA);
    //    --> fn(val1, ptrB);
    //   fn(val2, ptrB);
  */
  /* do actual optimizations */
  bool handleFunction(const fp583::InstLogAnalysis& instLogAnalysis, Function& f) {
    std::unordered_map<Function*, std::vector<CallBase*>> prevFunctionCalls;
    bool changed = false;

    for (auto& bb : f) {
      for (auto& inst : bb) {
        if (auto* currCall = dyn_cast<CallBase>(&inst)) {
          auto* fCalled = currCall->getCalledFunction();
          if (!isFunctionPure(fCalled)) continue;

          bool callNotDeleted = true;
          if (prevFunctionCalls.count(fCalled)) {
            auto& prevCallsVec = prevFunctionCalls[fCalled];
            for (auto* prevCall : prevCallsVec) {
              std::vector<std::pair<Value*, Value*>> ptrArgsVals;
              if (areFunctionCallsIdentical(instLogAnalysis, currCall, prevCall, ptrArgsVals)) {
                errs() << "optim function call: " << fCalled->getName() << "\n";
                removeFunctionCallAndFixUp(currCall, prevCall, ptrArgsVals);
                changed = true;
                callNotDeleted = false;
                break;
              }
            }
          }
          
          if (callNotDeleted) {
            prevFunctionCalls[fCalled].push_back(currCall);
          }
        }
      }
    }
    return changed;
  }

  bool runOnModule(Module &m) override {
    bool changed = false;
    auto& instLogAnalysis = getAnalysis<fp583::InstLogAnalysisWrapperPass>().getInstLogAnalysis();

    for (auto& f : m) {
      changed |= handleFunction(instLogAnalysis, f);
    }

    return changed;
  }
}; // end of struct OptOnAliasProfilePass
}  // end of anonymous namespace

char OptimOnAliasProfilePass::ID = 0;
static RegisterPass<OptimOnAliasProfilePass> x("fp_optim", "OptimOnAliasProfilePass Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

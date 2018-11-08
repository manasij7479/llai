#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <deque>
#include <unordered_map>

using namespace llvm;

namespace {
struct AbstractInterpreter : public FunctionPass {
  static char ID;
  AbstractInterpreter() : FunctionPass(ID) {}

  struct Range {
    Range() : R(llvm::ConstantRange(32)) {}
    Range(llvm::ConstantRange R_) : R(R_) {}
    llvm::ConstantRange R;
    bool operator==(const Range &Other) {
      return R == Other.R;
    }
    bool operator!=(const Range &Other) {
      return R != Other.R;
    }
  };

  bool runOnFunction(Function &F) override {
    InitWorkList(F);

    // dumpWorkList();

    while (!Worklist.empty()) {
      // dumpWorkList();
      auto I = Worklist.front();
      Worklist.pop_front();

      auto OldResult = Results[I];
      auto NewResult = ProcessValue(I);
      if (OldResult != NewResult) {
        Results[I] = NewResult;
        for(auto U : I->users()){
          if (auto UserV = dyn_cast<Value>(U)){
            Worklist.push_back(UserV);
          }
        }
      }
    }

    dumpResults();
    Clean();
    return false;
  }

private:
  Range ProcessValue(Value *V) {

    if (Results.find(V) != Results.end()) {
      return Results[V];
    }

    if (auto I = dyn_cast<llvm::Instruction>(V)) {
      return ProcessInstruction(I);
    } else if (auto C = dyn_cast<llvm::ConstantInt>(V)) {
      return Range(ConstantRange(C->getValue()));
    } else {
      // What goes here?
    }
  }
  Range ProcessInstruction (llvm::Instruction *I) {
    //auto Width = I->getType()->getIntegerBitWidth();
    auto Width = 32;
    ConstantRange FullRange(Width);
    if (I->isBinaryOp()) {
      auto Op1 = I->getOperand(0);
      auto Op2 = I->getOperand(1);
      auto R1 = ProcessValue(Op1).R;
      auto R2 = ProcessValue(Op2).R;

      ConstantRange Result = FullRange;

      switch (I->getOpcode()) {
        case llvm::Instruction::Add : {
          Result = R1.add(R2);
          break;
        }
        case llvm::Instruction::Sub : {
          Result = R1.sub(R2);
          break;
        }
        default: {/*no op*/}
      }
      return Range(Result);
    } else {
      // TODO: select, phi, ret etc.
    }
    return Range(FullRange);
  }
  void InitWorkList(Function &F) {
    for (auto &&BB : F) {
      for (auto &&I : BB) {
        Results[&I] = Range{ProcessInstruction(&I)};
        Worklist.push_back(&I);
      }
    }
  }
  void Clean() {
    Results.clear();
    Worklist = {};
  }
  void dumpResults() {
    for (auto p : Results) {
      llvm::outs() << "[";
      p.first->print(llvm::outs());
      llvm::outs() << "\t:\t";
      p.second.R.print(llvm::outs());
      llvm::outs() << "]\n";
    }
  }
  void dumpWorkList() {
    llvm::outs() << "[";
    for (auto V : Worklist) {
      V->print(llvm::outs());
      llvm::outs() << ", ";
    }
    llvm::outs() << "]\n";
  }
  std::unordered_map<llvm::Value *, Range> Results;
  std::deque<llvm::Value *> Worklist;
};
}  // end of anonymous namespace

char AbstractInterpreter::ID = 0;
static RegisterPass<AbstractInterpreter> X("llai", "Simple Abstract Interpreter",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

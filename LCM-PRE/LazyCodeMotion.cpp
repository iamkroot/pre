#include <llvm/IR/Function.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

using namespace llvm;

namespace {
struct LCM : public FunctionPass {
    static char ID;
    LCM() : FunctionPass(ID) {}

    bool runOnFunction(Function &F);
};
} // namespace

char LCM::ID = 0;
static RegisterPass<LCM> X("lcm", 
                           "Lazy Code Motion for Partial Redundancy Elimination Pass",
                           false /* Only looks at CFG */, 
                           false /* Also transforms the CFG */);

static RegisterStandardPasses Y(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM) { PM.add(new LCM()); });

bool LCM :: runOnFunction(Function &F)
{
    errs() << "Basic Blocks for Function '" << F.getName() << "':\n";
    // Iterate over basic blocks in the function
    for (BasicBlock &BB : F) {
        errs() << "Basic Block: " << BB.getName() << "\n";
        // Iterate over instructions in the basic block
        for (Instruction &I : BB) {
            errs() << "\t" << I << "\n";
        }
        errs() << "\n";
    }
    return false;
}
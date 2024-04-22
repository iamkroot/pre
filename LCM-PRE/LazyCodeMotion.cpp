#include "llvm/Pass.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "DataflowAnalyzer.h"
#include "Expression.h"
#include <iostream>
#include <map>
#include <vector>

using namespace llvm;

namespace {
    struct LCM : public FunctionPass {
        static char ID;
        LCM() : FunctionPass(ID) {}

        bool runOnFunction(Function &F);

        class PREDataFlowGlobalInfo: public DataflowInfo
        {
            public:
                std::map<Expression, int>  expressionsToIndexMap;
                std::vector<Expression*> expressions;
                std::set<BasicBlock*> exitBasicBlocks;
                std::set<BasicBlock*> allBasicBlocks;
                BasicBlock* entryBasicBlock;
                BitVector trueVector;
                BitVector falseVector;
                std::map<Value*, BitVector> varToExpsWithVarMap;
        };

        class PREDataFlowBlockLevelInfo: public DataflowInfo
        {   
            public:
                // Global properties.
                BitVector killAnticipated;
                BitVector genAnticipated;
                BitVector exprsUsed;
                BasicBlock* blockPtr;

                // Analysis specfic properties.
                BitVector anticipated[2];
                BitVector willBeAvailable[2];
                BitVector postponable[2];
                BitVector toBeUsed[2];
                BitVector earliest;
                BitVector latest;
        };

        private:
            PREDataFlowGlobalInfo* myGlobalDataflowInfo;
            std::map<BasicBlock*, PREDataFlowBlockLevelInfo*> myBlockLevelDataflowInfoMap;            

            // Helper functions
            bool isValidExprTypeForTracking(Instruction &instr);
            void addExpression(Expression* expr);
            int getExpressionIndex(Expression* expr);
            void updateGenAndKillForAnticipatedAnalysis(BasicBlock &block);
            void updateGenAndKillForUseAnalysis(BasicBlock &block);
            BitVector* getExpressionsWithVar(Value* var);
            void displayBitvector(BitVector bv);

            void initializeDS(Function &func);
            void splitIntoSingleInstructionBlocks(Function &func);
            void addNewBlocksOnCriticalEdges(Function &func);
            void gatherExpressionsFromFunction(Function &function);
            void calculateGlobalProperties(Function &func);
            void computeEarliest();
            void computeLatest();
    };
} // namespace

// ----------- Helper functions implementation -------------
bool LCM :: isValidExprTypeForTracking(Instruction &instr)
{
    return instr.isBinaryOp();
}

void LCM :: addExpression(Expression* expr)
{
    if(myGlobalDataflowInfo->expressionsToIndexMap.find(*expr) == myGlobalDataflowInfo->expressionsToIndexMap.end())
    {
        myGlobalDataflowInfo->expressions.push_back(expr);
        myGlobalDataflowInfo->expressionsToIndexMap[*expr] = myGlobalDataflowInfo->expressions.size() - 1;
    }
}

int LCM::getExpressionIndex(Expression* expr) {
    if(myGlobalDataflowInfo->expressionsToIndexMap.find(*expr) == myGlobalDataflowInfo->expressionsToIndexMap.end()) {
        return -1;
    }
    else {
        return myGlobalDataflowInfo->expressionsToIndexMap[*expr];
    }
}

BitVector* LCM::getExpressionsWithVar(Value* var) {
    if (myGlobalDataflowInfo->varToExpsWithVarMap.find(var) == myGlobalDataflowInfo->varToExpsWithVarMap.end()) {
        BitVector expsWithVarMap = BitVector(myGlobalDataflowInfo->expressions.size(), false);
        for (unsigned int i = 0; i< myGlobalDataflowInfo->expressions.size(); i++) {
            if (myGlobalDataflowInfo->expressions[i]->hasOperand(var)) {
                expsWithVarMap.set(i);
            }
        }
        
        // Cache for future references.
        myGlobalDataflowInfo->varToExpsWithVarMap[var] = expsWithVarMap;
    }

    return &myGlobalDataflowInfo->varToExpsWithVarMap[var];
}


void LCM :: updateGenAndKillForAnticipatedAnalysis(BasicBlock &block)
{
    PREDataFlowBlockLevelInfo &dfInfo = *(myBlockLevelDataflowInfoMap[&block]);
    dfInfo.blockPtr = &block;
    dfInfo.killAnticipated = BitVector(myGlobalDataflowInfo->expressions.size());
    dfInfo.genAnticipated = BitVector(myGlobalDataflowInfo->expressions.size());

    for(Instruction &instr: block) {
        if(isValidExprTypeForTracking(instr)) {
            Expression* expr = Expression::getExpression(instr);
            int index = getExpressionIndex(expr);

            // Output of the instruction.
            Value* dest = &instr;

            dfInfo.killAnticipated |= *(getExpressionsWithVar(dest));

            if(!dfInfo.killAnticipated[index]) {
                dfInfo.genAnticipated[index] = true;
            }
        }
    }
}

void LCM :: updateGenAndKillForUseAnalysis(BasicBlock &block)
{
    PREDataFlowBlockLevelInfo &dfInfo = *(myBlockLevelDataflowInfoMap[&block]);
    dfInfo.exprsUsed = BitVector(myGlobalDataflowInfo->expressions.size());

    for(Instruction &instr: block) {
        if(isValidExprTypeForTracking(instr)) {
            Expression* expr = Expression::getExpression(instr);
            int index = getExpressionIndex(expr);
            dfInfo.exprsUsed[index] = true;
        }
    }
}

void LCM :: displayBitvector(BitVector bv)
{
    errs() << "{";
    for(unsigned i = 0; i < bv.size(); i++) {
        errs() << (bv[i] ? '1' : '0');
    }
    errs() << "}\n";
}

// ------------ Main Algo Step Functions --------------
void LCM :: initializeDS(Function &func)
{
    myGlobalDataflowInfo = new PREDataFlowGlobalInfo();
    myGlobalDataflowInfo->expressionsToIndexMap.clear();
    myGlobalDataflowInfo->expressions.clear();
    myGlobalDataflowInfo->exitBasicBlocks.clear();
    myGlobalDataflowInfo->trueVector = BitVector();
    myGlobalDataflowInfo->falseVector = BitVector();
    myGlobalDataflowInfo->varToExpsWithVarMap.clear();

    for (BasicBlock &block: func) {
        PREDataFlowBlockLevelInfo* dfInfo = new PREDataFlowBlockLevelInfo();
        myBlockLevelDataflowInfoMap[&block] = dfInfo;
    }
}

void LCM :: splitIntoSingleInstructionBlocks(Function &func)
{
    std::vector<BasicBlock*> origBlocks;
    for (BasicBlock& block : func) {
        origBlocks.push_back(&block);
    }

    for (BasicBlock* blockPtr : origBlocks) {
        BasicBlock &block = *blockPtr;
        if (block.size() > 1) {
            for (int i = block.size() - 1; i > 0; i--) {
                if (i == block.size() - 1) {
                    SplitBlock(&block, &block.back());
                } else {
                    SplitBlock(&block, &(*(++block.rbegin())));
                }
            }
        }
    }
}

void LCM :: addNewBlocksOnCriticalEdges(Function &func)
{
    std::set<std::pair<BasicBlock*, BasicBlock*>> pairsToSplit;

    for (BasicBlock &block : func) {
        if (block.hasNPredecessorsOrMore(2)) {
            for (auto pred : predecessors(&block)) {
                pairsToSplit.insert(std::make_pair(pred, &block));
            }
        }
    }

    for (auto& splitPair : pairsToSplit) {
        SplitEdge(splitPair.first, splitPair.second);
    }
}

void LCM :: gatherExpressionsFromFunction(Function &function)
{
    for (BasicBlock &block: function) {
        for (Instruction &instr : block) {
            if(isValidExprTypeForTracking(instr))
            {
                Expression* expr = Expression::getExpression(instr);
                addExpression(expr);
            }
        }
    }

    int numExpressions = myGlobalDataflowInfo->expressionsToIndexMap.size();
    myGlobalDataflowInfo->trueVector = BitVector(numExpressions, true);
    myGlobalDataflowInfo->falseVector = BitVector(numExpressions);
}

void LCM :: calculateGlobalProperties(Function &func)
{
    // Exit and all basic blocks.
    for (BasicBlock &block: func) {
        myGlobalDataflowInfo->allBasicBlocks.insert(&block);
        if(block.getTerminator()->getNumSuccessors() == 0) {
            myGlobalDataflowInfo->exitBasicBlocks.insert(&block);
        }
    }

    myGlobalDataflowInfo->entryBasicBlock = &(func.getEntryBlock());

    // Block level global properties.
    for (BasicBlock &block : func)
    {
        updateGenAndKillForAnticipatedAnalysis(block);
        updateGenAndKillForUseAnalysis(block);
    }
}

void LCM :: computeEarliest()
{
    for(auto it = myBlockLevelDataflowInfoMap.begin(); it != myBlockLevelDataflowInfoMap.end(); it++) {
        BitVector earliest = it->second->anticipated[0];
        earliest.reset(it->second->willBeAvailable[0]);
        it->second->earliest = earliest;
    }
}

void LCM :: computeLatest()
{
    for(auto it = myBlockLevelDataflowInfoMap.begin(); it != myBlockLevelDataflowInfoMap.end(); it++) {
        BitVector latest = it->second->earliest;
        latest |= it->second->postponable[0];

        std::set<BasicBlock*> succBlocks;
        for(BasicBlock* succ: successors(it->first)) {
            succBlocks.insert(succ);
        }

        BitVector v1 = succBlocks.empty() ?  myGlobalDataflowInfo->falseVector :  myGlobalDataflowInfo->trueVector;
        for(BasicBlock* block: succBlocks)
        {
            BitVector v2 = myBlockLevelDataflowInfoMap[block]->earliest;
            v2 |= myBlockLevelDataflowInfoMap[block]->postponable[0];
            v1 &= v2;
        }
        v1.flip();
        v1 |= it->second->exprsUsed;

        latest &= v1;

        it->second->latest = latest;
    }
}
// ----------------- Dataflow Analyzer classes ------------------
class AnticipatedExpressionsAnalyzer: 
    public DataflowAnalyzer<LCM::PREDataFlowGlobalInfo, LCM::PREDataFlowBlockLevelInfo>
{
    public:
    AnticipatedExpressionsAnalyzer(
        LCM::PREDataFlowGlobalInfo* globalDataflowInfoMap,
        std::map<BasicBlock*, LCM::PREDataFlowBlockLevelInfo*> blockLevelDataflowInfoMap,
        bool isForwardAnalysis):
        DataflowAnalyzer(globalDataflowInfoMap, blockLevelDataflowInfoMap, isForwardAnalysis)
    {
        // no-op
    }

    void initializeBlockLevelDataflowInfo() override
    {
        for(auto it = myBlockLevelDataflowInfoMap.begin(); it != myBlockLevelDataflowInfoMap.end(); it++) {
            it->second->anticipated[0] = it->second->anticipated[1] = myGlobalDataflowInfo->trueVector;

            if(myGlobalDataflowInfo->exitBasicBlocks.find(it->first) != myGlobalDataflowInfo->exitBasicBlocks.end()) {
                it->second->anticipated[1] = myGlobalDataflowInfo->falseVector;
            }
        }
    }

    bool modifyBlockLevelInfo(BasicBlock* currentBlockPtr, std::set<BasicBlock*> inputBlocks) override
    {
        LCM::PREDataFlowBlockLevelInfo* blockInfo = myBlockLevelDataflowInfoMap[currentBlockPtr];

        BitVector newAnticipated[2];
        newAnticipated[0] = BitVector(blockInfo->anticipated[0]);
        newAnticipated[1] = BitVector(blockInfo->anticipated[1]);

        if (!inputBlocks.empty()) {
            newAnticipated[1] = myGlobalDataflowInfo->trueVector;
            for(BasicBlock* b: inputBlocks) {
                newAnticipated[1] &= myBlockLevelDataflowInfoMap[b]->anticipated[0];
            }
        }
        else {
            newAnticipated[1] = myGlobalDataflowInfo->falseVector;
        }

        newAnticipated[0] = newAnticipated[1];
        newAnticipated[0].reset(blockInfo->killAnticipated); // (out - kill)
        newAnticipated[0] |= (blockInfo->genAnticipated); // (out - kill) U Gen

        bool hasChanged = newAnticipated[0] != blockInfo->anticipated[0];

        blockInfo->anticipated[0] = newAnticipated[0];
        blockInfo->anticipated[1] = newAnticipated[1];


        return hasChanged;
    }
};

class WillBeAvailableExpressionAnalyzer: 
    public DataflowAnalyzer<LCM::PREDataFlowGlobalInfo, LCM::PREDataFlowBlockLevelInfo>
{
    public:
    WillBeAvailableExpressionAnalyzer(
        LCM::PREDataFlowGlobalInfo* globalDataflowInfoMap,
        std::map<BasicBlock*, LCM::PREDataFlowBlockLevelInfo*> blockLevelDataflowInfoMap,
        bool isForwardAnalysis):
        DataflowAnalyzer(globalDataflowInfoMap, blockLevelDataflowInfoMap, isForwardAnalysis)
    {
        // no-op
    }

    void initializeBlockLevelDataflowInfo() override
    {
        for(auto it = myBlockLevelDataflowInfoMap.begin(); it != myBlockLevelDataflowInfoMap.end(); it++) {
            it->second->willBeAvailable[0] = it->second->willBeAvailable[1] = myGlobalDataflowInfo->trueVector;

            if(it->first == myGlobalDataflowInfo->entryBasicBlock) {
                it->second->willBeAvailable[0] = myGlobalDataflowInfo->falseVector;
            }
        }
    }

    bool modifyBlockLevelInfo(BasicBlock* currentBlockPtr, std::set<BasicBlock*> inputBlocks) override
    {
        LCM::PREDataFlowBlockLevelInfo* blockInfo = myBlockLevelDataflowInfoMap[currentBlockPtr];

        BitVector newWillBeAvailable[2];
        newWillBeAvailable[0] = BitVector(blockInfo->willBeAvailable[0]);
        newWillBeAvailable[1] = BitVector(blockInfo->willBeAvailable[1]);

        if (!inputBlocks.empty()) {
            newWillBeAvailable[0] = myGlobalDataflowInfo->trueVector;
            for(BasicBlock* b: inputBlocks) {
                newWillBeAvailable[0] &= myBlockLevelDataflowInfoMap[b]->willBeAvailable[1];
            }
        }
        else {
            newWillBeAvailable[0] = myGlobalDataflowInfo->falseVector;
        }

        newWillBeAvailable[1] = newWillBeAvailable[0];
        newWillBeAvailable[1] |= blockInfo->anticipated[0];
        newWillBeAvailable[1].reset(blockInfo->killAnticipated);

        bool hasChanged = newWillBeAvailable[1] != blockInfo->willBeAvailable[1];

        blockInfo->willBeAvailable[0] = newWillBeAvailable[0];
        blockInfo->willBeAvailable[1] = newWillBeAvailable[1];

        return hasChanged;
    }
};

class PostponableAnalyzer: 
    public DataflowAnalyzer<LCM::PREDataFlowGlobalInfo, LCM::PREDataFlowBlockLevelInfo>
{
    public:
    PostponableAnalyzer(
        LCM::PREDataFlowGlobalInfo* globalDataflowInfoMap,
        std::map<BasicBlock*, LCM::PREDataFlowBlockLevelInfo*> blockLevelDataflowInfoMap,
        bool isForwardAnalysis):
        DataflowAnalyzer(globalDataflowInfoMap, blockLevelDataflowInfoMap, isForwardAnalysis)
    {
        // no-op
    }

    void initializeBlockLevelDataflowInfo() override
    {
        for(auto it = myBlockLevelDataflowInfoMap.begin(); it != myBlockLevelDataflowInfoMap.end(); it++) {
            it->second->postponable[0] = it->second->postponable[1] = myGlobalDataflowInfo->trueVector;

            if(it->first == myGlobalDataflowInfo->entryBasicBlock) {
                it->second->postponable[0] = myGlobalDataflowInfo->falseVector;
            }
        }
    }

    bool modifyBlockLevelInfo(BasicBlock* currentBlockPtr, std::set<BasicBlock*> inputBlocks) override
    {
        LCM::PREDataFlowBlockLevelInfo* blockInfo = myBlockLevelDataflowInfoMap[currentBlockPtr];

        BitVector newPostponable[2];
        newPostponable[0] = BitVector(blockInfo->postponable[0]);
        newPostponable[1] = BitVector(blockInfo->postponable[1]);

        if (!inputBlocks.empty()) {
            newPostponable[0] = myGlobalDataflowInfo->trueVector;
            for(BasicBlock* b: inputBlocks) {
                newPostponable[0] &= myBlockLevelDataflowInfoMap[b]->postponable[1];
            }
        }
        else {
            newPostponable[0] = myGlobalDataflowInfo->falseVector;
        }

        newPostponable[1] = newPostponable[0];
        newPostponable[1] |= blockInfo->earliest;
        newPostponable[1].reset(blockInfo->exprsUsed);

        bool hasChanged = newPostponable[1] != blockInfo->postponable[1];

        blockInfo->postponable[0] = newPostponable[0];
        blockInfo->postponable[1] = newPostponable[1];

        return hasChanged;
    }
};

class ToBeUsedExpressionsAnalyzer: 
    public DataflowAnalyzer<LCM::PREDataFlowGlobalInfo, LCM::PREDataFlowBlockLevelInfo>
{
    public:
    ToBeUsedExpressionsAnalyzer(
        LCM::PREDataFlowGlobalInfo* globalDataflowInfoMap,
        std::map<BasicBlock*, LCM::PREDataFlowBlockLevelInfo*> blockLevelDataflowInfoMap,
        bool isForwardAnalysis):
        DataflowAnalyzer(globalDataflowInfoMap, blockLevelDataflowInfoMap, isForwardAnalysis)
    {
        // no-op
    }

    void initializeBlockLevelDataflowInfo() override
    {
        for(auto it = myBlockLevelDataflowInfoMap.begin(); it != myBlockLevelDataflowInfoMap.end(); it++) {
            it->second->toBeUsed[0] = it->second->toBeUsed[1] = myGlobalDataflowInfo->falseVector;
        }
    }

    bool modifyBlockLevelInfo(BasicBlock* currentBlockPtr, std::set<BasicBlock*> inputBlocks) override
    {
        LCM::PREDataFlowBlockLevelInfo* blockInfo = myBlockLevelDataflowInfoMap[currentBlockPtr];

        BitVector newToBeUsed[2];
        newToBeUsed[0] = BitVector(blockInfo->toBeUsed[0]);
        newToBeUsed[1] = BitVector(blockInfo->toBeUsed[1]);

        newToBeUsed[1] = myGlobalDataflowInfo->falseVector;
        for(BasicBlock* b: inputBlocks) {
            newToBeUsed[1] |= myBlockLevelDataflowInfoMap[b]->toBeUsed[0];
        }

        newToBeUsed[0] = newToBeUsed[1];
        newToBeUsed[0] |= blockInfo->exprsUsed;
        newToBeUsed[0].reset(blockInfo->latest);

        bool hasChanged = newToBeUsed[0] != blockInfo->toBeUsed[0];

        blockInfo->toBeUsed[0] = newToBeUsed[0];
        blockInfo->toBeUsed[1] = newToBeUsed[1];


        return hasChanged;
    }
};

char LCM::ID = 0;
static RegisterPass<LCM> X("lcm", 
                           "Lazy Code Motion for Partial Redundancy Elimination Pass",
                           false /* Only looks at CFG */, 
                           false /* Also transforms the CFG */);

static RegisterStandardPasses Y(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM) { PM.add(new LCM()); });


bool LCM :: runOnFunction(Function &func)
{
    splitIntoSingleInstructionBlocks(func);

    addNewBlocksOnCriticalEdges(func);

    initializeDS(func);

    gatherExpressionsFromFunction(func);

    calculateGlobalProperties(func);

    AnticipatedExpressionsAnalyzer analyzer(myGlobalDataflowInfo, myBlockLevelDataflowInfoMap, false);
    analyzer.execute();

    WillBeAvailableExpressionAnalyzer analyzer1(myGlobalDataflowInfo, myBlockLevelDataflowInfoMap, true);
    analyzer1.execute();

    computeEarliest();

    PostponableAnalyzer analyzer2(myGlobalDataflowInfo, myBlockLevelDataflowInfoMap, true);
    analyzer2.execute();

    computeLatest();

    ToBeUsedExpressionsAnalyzer analyzer3(myGlobalDataflowInfo, myBlockLevelDataflowInfoMap, false);
    analyzer3.execute();

    errs() << "Number of expressions: " <<  myGlobalDataflowInfo->expressions.size() << "\n";

    errs() << "Basic Blocks for Function '" << func.getName() << "':\n";
    // Iterate over basic blocks in the function
    int x = 0;
    for (BasicBlock &basicBlock : func) {
        errs() << "Basic Block: " << basicBlock.getName().str() << "\n";
        // Iterate over instructions in the basic block
        for (Instruction &instr : basicBlock) {
            errs() << "\t" << instr << "\n";
        }
        errs() << "\n";
        x += 1;

        displayBitvector(myBlockLevelDataflowInfoMap[&basicBlock]->latest);
    }
    errs() << x << "\n";


    return false;
}
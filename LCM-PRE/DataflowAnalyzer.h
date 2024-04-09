#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include <map>
#include <queue>
#include <set>

using namespace llvm;

class DataflowInfo {
    // public:
    // virtual void display() = 0;
};

template<typename T1, typename T2>
class DataflowAnalyzer {
public:
    DataflowAnalyzer(T1* globalDataFlowInfo, std::map<BasicBlock*, T2*> blockLevelDataflowInfoMap, bool isForwardAnalysis);
    void execute();

    virtual void initializeBlockLevelDataflowInfo() = 0;
    virtual bool modifyBlockLevelInfo(BasicBlock* currentBlockPtr, std::set<BasicBlock*> inputBlocks) = 0;

    static std::set<BasicBlock*> getSuccessors(BasicBlock* currentBlockPtr);
    static std::set<BasicBlock*> getPredecessors(BasicBlock* currentBlockPtr);

protected:
    T1* myGlobalDataflowInfo;
    std::map<BasicBlock*, T2*> myBlockLevelDataflowInfoMap;
    bool myIsForwardAnalysis;
};

// ------------------------- Implementation --------------------------------
template<typename T1, typename T2>
DataflowAnalyzer<T1, T2> :: DataflowAnalyzer(T1* globalDataFlowInfo, 
                                             std::map<BasicBlock*, T2*> blockLevelDataflowInfoMap,
                                             bool isForwardAnalysis)
{
    myGlobalDataflowInfo = globalDataFlowInfo;
    myBlockLevelDataflowInfoMap = blockLevelDataflowInfoMap;
    myIsForwardAnalysis = isForwardAnalysis;
}

template<typename T1, typename T2>
std::set<BasicBlock*> DataflowAnalyzer<T1, T2> :: getSuccessors(BasicBlock* currentBlockPtr)
{
    std::set<BasicBlock*> succBlocks;
    for(BasicBlock* succ: successors(currentBlockPtr)) {
        succBlocks.insert(succ);
    }
    return succBlocks;
}

template<typename T1, typename T2>
std::set<BasicBlock*> DataflowAnalyzer<T1, T2> :: getPredecessors(BasicBlock* currentBlockPtr)
{
    std::set<BasicBlock*> predBlocks;
    for(BasicBlock* pred: predecessors(currentBlockPtr)) {
        predBlocks.insert(pred);
    }
    return predBlocks;
}

template<typename T1, typename T2>
void DataflowAnalyzer<T1, T2> :: execute()
{
    // Initialize the data structures to be used for this specific analysis.
    initializeBlockLevelDataflowInfo();

    std::queue<BasicBlock*> blockQueue;
    std::set<BasicBlock*> blockVisitedSet;

    for (BasicBlock* blockPtr: myGlobalDataflowInfo->allBasicBlocks) {
        blockQueue.push(blockPtr);
        blockVisitedSet.insert(blockPtr);
    }

    int numIters = 0;
    while (!blockQueue.empty()) {
        BasicBlock* currentBlockPtr = blockQueue.front();
        blockQueue.pop();
        blockVisitedSet.erase(currentBlockPtr);
        numIters += 1;

        std::set<BasicBlock*> inputBlocks;
        std::set<BasicBlock*> outpBlocks;

        if (myIsForwardAnalysis) {
            inputBlocks = DataflowAnalyzer<T1, T2> :: getPredecessors(currentBlockPtr);
            outpBlocks = DataflowAnalyzer<T1, T2> :: getSuccessors(currentBlockPtr);
        }
        else {
            inputBlocks = DataflowAnalyzer<T1, T2> :: getSuccessors(currentBlockPtr);
            outpBlocks = DataflowAnalyzer<T1, T2> :: getPredecessors(currentBlockPtr);
        }

        if(modifyBlockLevelInfo(currentBlockPtr, inputBlocks)) {
            // Add the output nodes to the queue if they are not already in the visited set.
            for (BasicBlock* b: outpBlocks) {
                if(blockVisitedSet.find(b) == blockVisitedSet.end()) {
                    blockQueue.push(b);
                    blockVisitedSet.insert(b);
                }
            }
        }
    }
    errs() << "Num Iterations: " << numIters << "\n";
}
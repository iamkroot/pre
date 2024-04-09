#include <string>
#include <vector>
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"
using namespace llvm;

class Expression {
public:
    unsigned opCode;
    Type* type = nullptr;
    std::vector<Value*> &args;
    Instruction *instr;

    Expression(unsigned opCode, Type* type, std::vector<Value *> &args, Instruction *instr): opCode(opCode), type(type), args(args), instr(instr){};
    static Expression* getExpression(Instruction &instr);
    bool operator<(const Expression &exp) const;
    bool hasOperand(Value* var) const;
};
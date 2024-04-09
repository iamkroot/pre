#include "Expression.h"

Expression* Expression::getExpression(Instruction &instr) {
    // errs() << "generating an exp:" << "\n";
    unsigned opCode = instr.getOpcode();
    Type *type = instr.getType();
    std::vector<Value*> *args = new std::vector<Value*>();
    // errs() << "JMD1" << instr << "\n";
    // errs() << "JMD2" << instr.getOpcodeName() << ": " << instr.getName() << "/" << *type << "\n";
    for (auto& operand : instr.operands()) {
        // errs() << "JMD3" << *operand << ": " << operand->getName() << "\n";
        args->push_back(operand);
    }

    return new Expression(opCode, type, *args, &instr);
}


bool Expression::operator<(const Expression &exp) const {
    // errs() << "comparing exps" << "\n";
    // errs() << *this->instr << "\n";
    // errs() << *exp.instr << "\n";

    if (opCode != exp.opCode) {
        return opCode < exp.opCode;
    }
    else if (type->getTypeID() != exp.type->getTypeID()) {
        return type->getTypeID() < exp.type->getTypeID();
    }
    else if (args.size() != exp.args.size()) {
        return args.size() < exp.args.size();
    }
    else {
        for (int i = 0; i < args.size(); ++i) {
            // errs() << args[i] << "\n" << exp.args[i] << "\n";
            if (args[i] != exp.args[i]) {
                // errs() << "wy?\n";
                return args[i] < exp.args[i];
            }
        }
    }

    return false;
}


bool Expression::hasOperand(Value* var) const {
    for (Use &U : instr->operands()) {
        if (var == U.get()) {
            return true;
        }
    }
    return false;
}
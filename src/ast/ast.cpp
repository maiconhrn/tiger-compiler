#include "ast.hpp"
#include <llvm/IR/DerivedTypes.h>
#include "utils/symboltable.hpp"
#include <iostream>

using namespace AST;
using namespace std;

static void blank(int n) {
    for (int i = 0; i < n; i++) {
        cout << "___";
    }
}

llvm::Type *Root::traverse(vector<VarDec *> &, CodeGenContext &context) {
    context.typeDecs.reset();
    return root_->traverse(mainVariableTable_, context);
}

llvm::Type *AST::FieldVar::traverse(vector<VarDec *> &variableTable,
                                    CodeGenContext &context) {
    auto var = var_->traverse(variableTable, context);

    if (!var) {
        return nullptr;
    }

    if (!context.isRecord(var)) {
        return context.logErrorT("field reference is only for record type.",
                                 var_->getLocation());
    }

    llvm::StructType *eleType =
            llvm::cast<llvm::StructType>(context.getElementType(var));

    auto typeDec =
            dynamic_cast<RecordType *>(context.typeDecs[eleType->getStructName()]);
    assert(typeDec);

    idx_ = 0u;
    for (auto &field : typeDec->fields_) {
        if (field->getName() == field_.getName()) {
            break;
        } else {
            ++idx_;
        }
    }

    if (idx_ >= typeDec->fields_.size()) {
        return context.logErrorT("field not exists in this struct",
                                 var_->getLocation());
    }

    type_ = typeDec->fields_[idx_]->getType();
    return type_;
}

llvm::Type *AST::SubscriptVar::traverse(vector<VarDec *> &variableTable,
                                        CodeGenContext &context) {
    auto var = var_->traverse(variableTable, context);
    if (!var) {
        return nullptr;
    }
    if (!var->isPointerTy() || context.getElementType(var)->isStructTy()) {
        return context.logErrorT("Subscript is only for array type", getLocation());
    }

    type_ = context.getElementType(var);
    auto exp = exp_->traverse(variableTable, context);
    if (!exp) {
        return nullptr;
    }
    if (!exp->isIntegerTy()) {
        return context.logErrorT("Subscript should be integer", getLocation());
    }

    return type_;
}

llvm::Type *AST::VarExp::traverse(vector<VarDec *> &variableTable,
                                  CodeGenContext &context) {
    return var_->traverse(variableTable, context);
}

llvm::Type *AST::NilExp::traverse(vector<VarDec *> &, CodeGenContext &context) {
    return context.nilType;
}

llvm::Type *AST::IntExp::traverse(vector<VarDec *> &, CodeGenContext &context) {
    return context.intType;
}

llvm::Type *AST::StringExp::traverse(vector<VarDec *> &,
                                     CodeGenContext &context) {
    return context.stringType;
}


llvm::Type *CallExp::traverse(vector<VarDec *> &variableTable,
                              CodeGenContext &context) {
    auto function = context.functions[func_.getName()];
    if (!function) {
        return context.logErrorT("Function " +
                                 func_.getName() +
                                 " undeclared", getLocation());
    }
    auto functionType = function->getFunctionType();

    size_t i = 0u;
    if (function->getLinkage() == llvm::Function::ExternalLinkage) {
        i = 0u;
    } else {
        i = 1u;
    }

    if (args_.size() != functionType->getNumParams() - i) {
        return context.logErrorT("Incorrect number of passed arguments",
                                 getLocation());
    }

    for (auto &exp : args_) {
        auto type = exp->traverse(variableTable, context);
        auto arg = functionType->getParamType(i++);
        if (arg != type) {
            return context.logErrorT("Params type not match",
                                     getLocation());
        }
    }

    return function->getReturnType();
}

llvm::Type *BinaryExp::traverse(vector<VarDec *> &variableTable,
                                CodeGenContext &context) {
    auto left = left_->traverse(variableTable, context);
    auto right = right_->traverse(variableTable, context);
    if (!left || !right) return nullptr;
    switch (this->op_) {
        case ADD:
        case SUB:
        case MUL:
        case DIV:
        case LTH:
        case GTH:
        case LEQ:
        case GEQ: {
            if (left->isIntegerTy() && right->isIntegerTy())
                return context.intType;
            else
                return context.logErrorT("Binary expression require integers", getLocation());
        }
        case EQU:
        case NEQU: {
            if (context.isMatch(left, right)) {
                if (context.isNil(left) && context.isNil(right))
                    return context.logErrorT("Nil cannot compaire to nil", getLocation());
                else
                    return context.intType;
            } else
                return context.logErrorT("Binary comparasion type not match", getLocation());
        }
        default:
            return nullptr;
    }
    return nullptr;
}

llvm::Type *Field::traverse(vector<VarDec *> &variableTable,
                            CodeGenContext &context) {
    type_ = context.typeOf(getLocation(), typeName_.getName());
    varDec_ = new VarDec(getLocation(),
                         name_, type_,
                         variableTable.size(), context.currentLevel);
    context.valueDecs.push(name_.getName(), varDec_);
    variableTable.push_back(varDec_);
    return type_;
}

llvm::Type *FieldExp::traverse(vector<VarDec *> &variableTable,
                               CodeGenContext &context) {
    type_ = exp_->traverse(variableTable, context);
    return type_;
}

llvm::Type *RecordExp::traverse(vector<VarDec *> &variableTable,
                                CodeGenContext &context) {
    type_ = context.typeOf(typeName_->getLocation(), typeName_->getName().getName());

    if (!type_->isPointerTy()) {
        return context.logErrorT("Require a record type", typeName_->getLocation());
    }

    auto eleType = context.getElementType(type_);
    if (!eleType->isStructTy()) {
        return context.logErrorT("Require a record type", typeName_->getLocation());
    }

    auto typeDec = dynamic_cast<RecordType *>(context.typeDecs[typeName_->getName().getName()]);
    assert(typeDec);
    if (typeDec->fields_.size() != fieldExps_.size())
        return context.logErrorT("Wrong number of fields", getLocation());
    size_t idx = 0u;
    for (auto &fieldDec : typeDec->fields_) {
        auto &field = fieldExps_[idx];
        if (field->getName() != fieldDec->getName())
            return context.logErrorT(
                    field->getName() +
                    " is not a field or not on the right position of "
                    + typeName_->getName().getName(), getLocation());
        auto exp = field->traverse(variableTable, context);
        if (!context.isMatch(exp, fieldDec->getType()))
            return context.logErrorT("Field type not match", getLocation());
        ++idx;
    }
    // CHECK NIL
    return type_;
}


llvm::Type *SequenceExp::traverse(vector<VarDec *> &variableTable,
                                  CodeGenContext &context) {
    llvm::Type *last;
    for (auto &exp : exps_) {
        last = exp->traverse(variableTable, context);
    }
    return last;
}


llvm::Type *AssignExp::traverse(vector<VarDec *> &variableTable,
                                CodeGenContext &context) {
    auto var = var_->traverse(variableTable, context);
    if (!var) return nullptr;
    auto exp = exp_->traverse(variableTable, context);
    if (!exp) return nullptr;
    if (context.isMatch(var, exp))
        return exp;
    else
        return context.logErrorT("Assign types do not match", getLocation());
}

llvm::Type *IfExp::traverse(vector<VarDec *> &variableTable,
                            CodeGenContext &context) {
    auto test = test_->traverse(variableTable, context);
    if (!test) return nullptr;
    auto then = then_->traverse(variableTable, context);
    if (!then) return nullptr;
    if (!test->isIntegerTy()) return context.logErrorT("Require integer in test", getLocation());
    if (else_) {
        auto elsee = else_->traverse(variableTable, context);
        if (!elsee) return nullptr;
        if (!context.isMatch(then, elsee))
            return context.logErrorT("Require same type in both branch", getLocation());
    } else {
        return context.voidType;
    }
    return then;
}

llvm::Type *WhileExp::traverse(vector<VarDec *> &variableTable,
                               CodeGenContext &context) {
    auto test = test_->traverse(variableTable, context);
    if (!test) return nullptr;
    if (!test->isIntegerTy()) return context.logErrorT("Require integer", test_->getLocation());
    body_->traverse(variableTable, context);
    return context.voidType;
}

llvm::Type *DoWhileExp::traverse(vector<VarDec *> &variableTable,
                                 CodeGenContext &context) {
    auto test = test_->traverse(variableTable, context);
    if (!test) return nullptr;
    if (!test->isIntegerTy()) return context.logErrorT("Require integer", test_->getLocation());
    body_->traverse(variableTable, context);
    return context.voidType;
}

llvm::Type *ForExp::traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) {
    auto low = low_->traverse(variableTable, context);
    if (!low) {
        return nullptr;
    }

    auto high = high_->traverse(variableTable, context);
    if (!high) {
        return nullptr;
    }

    if (!low->isIntegerTy() || !high->isIntegerTy()) {
        return context.logErrorT("For bounds require integer", getLocation());
    }

    varDec_ = new VarDec(getLocation(),
                         var_, context.intType,
                         variableTable.size(), context.currentLevel);
    variableTable.push_back(varDec_);
    context.valueDecs.push(var_.getName(), varDec_);
    auto body = body_->traverse(variableTable, context);
    if (!body) {
        return nullptr;
    }

    return context.voidType;
}


llvm::Type *AST::BreakExp::traverse(vector<VarDec *> &,
                                    CodeGenContext &context) {
    return context.voidType;
}


llvm::Type *LetExp::traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) {
    context.types.enter();
    context.typeDecs.enter();
    context.valueDecs.enter();
    context.functions.enter();
    for (auto &dec : decs_) {
        dec->traverse(variableTable, context);
    }
    auto body = body_->traverse(variableTable, context);
    context.functions.exit();
    context.valueDecs.exit();
    context.types.exit();
    context.typeDecs.exit();
    return body;
}

llvm::Type *TypeDec::traverse(vector<VarDec *> &, CodeGenContext &context) {
    type_->setName(name_);
    context.typeDecs[name_.getName()] = type_.get();
    return context.voidType;
}

llvm::Type *ArrayExp::traverse(vector<VarDec *> &variableTable,
                               CodeGenContext &context) {
    type_ = context.typeOf(getLocation(), typeName_->getName().getName());

    if (!type_) {
        return nullptr;
    }
    if (!type_->isPointerTy()) {
        return context.logErrorT("Array type required", getLocation());
    }

    auto eleType = context.getElementType(type_);
    auto init = init_->traverse(variableTable, context);
    if (!init) {
        return nullptr;
    }

    auto size = size_->traverse(variableTable, context);
    if (!size) {
        return nullptr;
    }
    if (!size->isIntegerTy()) {
        return context.logErrorT("Size should be integer", getLocation());
    }

    if (eleType != init) {
        return context.logErrorT("Initial type not matches", getLocation());
    }

    return type_;
}


llvm::FunctionType *Prototype::traverse(vector<VarDec *> &variableTable,
                                        CodeGenContext &context) {
    std::vector<llvm::Type *> args;
    auto linkType = llvm::PointerType::getUnqual(context.staticLink.front());
    args.push_back(linkType);
    frame = llvm::StructType::create(context.context, name_.getName() + "Frame");
    staticLink_ = new VarDec(getLocation(),
                             Identifier(getLocation(),
                                        "staticLink"),
                             linkType, variableTable.size(),
                             context.currentLevel);
    variableTable.push_back(staticLink_);
    for (auto &field : params_) {
        args.push_back(field->traverse(variableTable, context));
        if (!args.back()) {
            return nullptr;
        }
    }

    if (result_.getName().empty()) {
        resultType_ = llvm::Type::getVoidTy(context.context);
    } else {
        resultType_ = context.typeOf(getLocation(), result_.getName());
    }

    if (!resultType_) {
        return nullptr;
    }
    auto functionType = llvm::FunctionType::get(resultType_, args, false);
    function_ =
            llvm::Function::Create(functionType, llvm::Function::InternalLinkage,
                                   name_.getName(), context.module.get());
    return functionType;
}

llvm::Type *FunctionDec::traverse(vector<VarDec *> &, CodeGenContext &context) {
    if (context.functions.lookupOne(name_.getName()))
        return context.logErrorT("Function " + name_.getName() +
                                 " is already defined in same scope.", getLocation());
    context.valueDecs.enter();
    level_ = ++context.currentLevel;
    auto proto = proto_->traverse(variableTable_, context);
    if (!proto) return nullptr;
    context.staticLink.push_front(proto_->getFrame());
    context.functions.push(name_.getName(), proto_->getFunction());
    auto body = body_->traverse(variableTable_, context);
    context.staticLink.pop_front();
    if (!body) return nullptr;
    context.valueDecs.exit();
    --context.currentLevel;
    auto retType = proto_->getResultType();
    if (!retType->isVoidTy() && retType != body)
        return context.logErrorT("Function return type not match", getLocation());
    return context.voidType;
}

llvm::Type *SimpleVar::traverse(vector<VarDec *> &, CodeGenContext &context) {
    // TODO: check
    auto type = context.valueDecs[name_.getName()];
    if (!type) {
        return context.logErrorT(name_.getName()
                                 + " is not defined", getLocation());
    }
    return type->getType();
}

llvm::Type *VarDec::traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) {
    if (context.valueDecs.lookupOne(name_.getName()))
        return context.logErrorT(name_.getName()
                                 + " is already defined in this scope.",
                                 getLocation());
    offset_ = variableTable.size();
    level_ = context.currentLevel;
    variableTable.push_back(this);
    auto init = init_->traverse(variableTable, context);

    if (!typeName_) {
        type_ = init;
    } else {
        type_ = context.typeOf(typeName_->getName().getLoc(),
                               typeName_->getName().getName());;

        if (!context.isMatch(type_, init)) {
            return context.logErrorT("Type not match", typeName_->getLocation());
        }
    }

    if (!type_) {
        return nullptr;
    }

    context.valueDecs.push(name_.getName(), this);
    return context.voidType;
}

llvm::Type *AST::ArrayType::traverse(std::set<std::string> &parentName,
                                     CodeGenContext &context) {
    if (context.types[name_.getName()]) {
        return context.types[name_.getName()];
    }

    if (parentName.find(name_.getName()) != parentName.end()) {
        return context.logErrorT(name_.getName()
                                 + " has an endless loop of type define",
                                 getLocation());
    }

    parentName.insert(name_.getName());
    auto type = context.typeOf(getLocation(), type_.getName(), parentName);
    parentName.erase(name_.getName());
    if (!type) {
        return nullptr;
    }

    type = llvm::PointerType::getUnqual(type);
    context.types.push(name_.getName(), type);
    return type;
}

llvm::Type *AST::NameType::traverse(std::set<std::string> &parentName,
                                    CodeGenContext &context) {
    if (auto type = context.types[name_.getName()]) {
        return type;
    }

    if (auto type = context.types[getName().getName()]) {
        context.types.push(name_.getName(), type);
        return type;
    }

    if (parentName.find(name_.getName()) != parentName.end()) {
        return context.logErrorT(name_.getName() + " has an endless loop of type define",
                                 getLocation());
    }

    parentName.insert(name_.getName());

    auto type = context.typeOf(getLocation(), type_.getName(), parentName);
    parentName.erase(name_.getName());
    return type;
}

llvm::Type *AST::RecordType::traverse(std::set<std::string> &parentName,
                                      CodeGenContext &context) {
    if (context.types[name_.getName()]) return context.types[name_.getName()];
    std::vector<llvm::Type *> types;
    if (parentName.find(name_.getName()) != parentName.end()) {
        auto type = llvm::PointerType::getUnqual(
                llvm::StructType::create(context.context, name_.getName()));
        context.types.push(name_.getName(), type);
        return type;
    }
    parentName.insert(name_.getName());
    for (auto &field : fields_) {
        auto type = context.typeOf(getLocation(), field->typeName_.getName(), parentName);
        if (!type) return nullptr;
        field->type_ = type;
        types.push_back(type);
    }
    parentName.erase(name_.getName());
    if (auto type = context.types[name_.getName()]) {
        if (!type->isPointerTy()) return nullptr;
        auto eleType = context.getElementType(type);
        if (!eleType->isStructTy()) return nullptr;
        llvm::cast<llvm::StructType>(eleType)->setBody(types);
        return type;
    } else {
        type = llvm::PointerType::getUnqual(
                llvm::StructType::create(context.context, types, name_.getName()));
        if (!type) return nullptr;
        context.types.push(name_.getName(), type);
        return type;
    }
}

bool Root::semanticAnalisys() {
    CodeGenContext codeGenContext;
    traverse(mainVariableTable_, codeGenContext);

    return !codeGenContext.hasError;
}

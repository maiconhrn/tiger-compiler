#include "ast.hpp"
#include <llvm/IR/DerivedTypes.h>
#include "utils/symboltable.hpp"
#include <iostream>
#include <sstream>

using namespace AST;
using namespace std;

static void blank(int n) {
    for (int i = 0; i < n; i++) {
        cout << "___";
    }
}

static string getTabs(size_t depth) {
    stringstream tabs;

    for (int i = 0; i < depth; ++i) {
        tabs << AST::TAB;
    }

    return tabs.str();
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
                                 var_->getLoc());
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
                                 var_->getLoc());
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
        return context.logErrorT("Subscript is only for array type",
                                 var_->getLoc());
    }

    type_ = context.getElementType(var);
    auto exp = exp_->traverse(variableTable, context);
    if (!exp) {
        return nullptr;
    }
    if (!exp->isIntegerTy()) {
        return context.logErrorT("Subscript should be integer",
                                 var_->getLoc());
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
                                 " undeclared", func_.getLoc());
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
                                 getLoc());
    }

    for (auto &exp : args_) {
        auto type = exp->traverse(variableTable, context);
        auto arg = functionType->getParamType(i++);
        if (arg != type) {
            return context.logErrorT("Params type not match",
                                     exp->getLoc());
        }
    }

    return function->getReturnType();
}

llvm::Type *BinaryExp::traverse(vector<VarDec *> &variableTable,
                                CodeGenContext &context) {
    auto left = left_->traverse(variableTable, context);
    auto right = right_->traverse(variableTable, context);
    if (!left || !right) {
        return nullptr;
    }

    switch (this->op_) {
        case ADD:
        case SUB:
        case MUL:
        case DIV:
        case LTH:
        case GTH:
        case LEQU:
        case GEQU: {
            if (left->isIntegerTy() && right->isIntegerTy()) {
                return context.intType;
            } else {
                return context.logErrorT("Binary expression require integers", getLoc());
            }
        }
        case EQU:
        case NEQU: {
            if (context.isMatch(left, right)) {
                if (context.isNil(left) && context.isNil(right))
                    return context.logErrorT("Nil cannot compaire to nil", getLoc());
                else
                    return context.intType;
            } else
                return context.logErrorT("Binary comparasion type not match", getLoc());
        }
        default:
            return nullptr;
    }
}

llvm::Type *Field::traverse(vector<VarDec *> &variableTable,
                            CodeGenContext &context) {
    type_ = context.typeOf(getLoc(), typeName_.getName());
    varDec_ = new VarDec(getLoc(),
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
    type_ = context.typeOf(typeName_->getLoc(), typeName_->getName().getName());

    if (!type_) {
        return nullptr;
    }

    if (!type_->isPointerTy()) {
        return context.logErrorT("Require a record type", typeName_->getLoc());
    }

    auto eleType = context.getElementType(type_);
    if (!eleType->isStructTy()) {
        return context.logErrorT("Require a record type", typeName_->getLoc());
    }

    auto typeDec = dynamic_cast<RecordType *>(context.typeDecs[typeName_->getName().getName()]);
    assert(typeDec);
    if (typeDec->fields_.size() != fieldExps_.size())
        return context.logErrorT("Wrong number of fields", getLoc());
    size_t idx = 0u;
    for (auto &fieldDec : typeDec->fields_) {
        auto &field = fieldExps_[idx];
        if (field->getName() != fieldDec->getName())
            return context.logErrorT(
                    field->getName() +
                    " is not a field or not on the right position of "
                    + typeName_->getName().getName(), getLoc());
        auto exp = field->traverse(variableTable, context);
        if (!context.isMatch(exp, fieldDec->getType()))
            return context.logErrorT("Field type not match", fieldDec->getLoc());
        ++idx;
    }
    // CHECK NIL
    return type_;
}

llvm::Type *SequenceExp::traverse(vector<VarDec *> &variableTable,
                                  CodeGenContext &context) {
    llvm::Type *last = context.voidType;

    for (auto &exp : exps_) {
        last = exp->traverse(variableTable, context);
    }

    return last;
}

llvm::Type *AssignExp::traverse(vector<VarDec *> &variableTable,
                                CodeGenContext &context) {
    auto var = var_->traverse(variableTable, context);
    if (!var) {
        return nullptr;
    }

    auto exp = exp_->traverse(variableTable, context);
    if (!exp) {
        return nullptr;
    }

    if (context.isMatch(var, exp)) {
        return context.voidType;
    } else {
        return context.logErrorT("Assign types do not match", exp_->getLoc());
    }
}

llvm::Type *IfExp::traverse(vector<VarDec *> &variableTable,
                            CodeGenContext &context) {
    auto test = test_->traverse(variableTable, context);
    if (!test) {
        return nullptr;
    }

    auto then = then_->traverse(variableTable, context);
    if (!then) {
        return nullptr;
    }

    if (!test->isIntegerTy()) {
        return context.logErrorT("Require integer in test", test_->getLoc());
    }

    if (else_) {
        auto elsee = else_->traverse(variableTable, context);
        if (!elsee) {
            return nullptr;
        }

        if (!context.isMatch(then, elsee)) {
            return context.logErrorT("Require same type in both branch", else_->getLoc());
        }
    } else if (!then->isVoidTy()) {
        return context.logErrorT("\"Then\" returns a value but \"Else\" doesnt", then_->getLoc());
    } else {
        return context.voidType;
    }

    return then;
}

llvm::Type *WhileExp::traverse(vector<VarDec *> &variableTable,
                               CodeGenContext &context) {
    auto test = test_->traverse(variableTable, context);
    if (!test) {
        return nullptr;
    }

    if (!test->isIntegerTy()) {
        return context.logErrorT("Require integer", test_->getLoc());
    }

    auto bodyType = body_->traverse(variableTable, context);

    if (bodyType != context.voidType) {
        return context.logErrorT("While Body do not returns value", body_->getLoc());
    }

    return context.voidType;
}

llvm::Type *DoWhileExp::traverse(vector<VarDec *> &variableTable,
                                 CodeGenContext &context) {
    auto test = test_->traverse(variableTable, context);
    if (!test) {
        return nullptr;
    }

    if (!test->isIntegerTy()) {
        return context.logErrorT("Require integer", test_->getLoc());
    }

    auto bodyType = body_->traverse(variableTable, context);

    if (bodyType != context.voidType) {
        return context.logErrorT("While Body do not returns value", body_->getLoc());
    }

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
        return context.logErrorT("For bounds require integer", getLoc());
    }

    varDec_ = new VarDec(getLoc(),
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

//    for (auto &dec : decs_) {
//        dec->computeHeaders(variableTable, context);
//    }

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
    if (context.typeDecs.lookupOne(name_.getName())) {
        return context.logErrorT("Type "
                                 + name_.getName()
                                 + " is already defined in same scope.",
                                 name_.getLoc());
    }

    type_->setName(name_);
    context.typeDecs[name_.getName()] = type_.get();

    return context.voidType;
}

llvm::Type *ArrayExp::traverse(vector<VarDec *> &variableTable,
                               CodeGenContext &context) {
    type_ = context.typeOf(getLoc(), typeName_->getName().getName());
    if (!type_) {
        return nullptr;
    }

    if (!type_->isPointerTy()) {
        return context.logErrorT("Array type required", typeName_->getLoc());
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
        return context.logErrorT("Size should be integer", size_->getLoc());
    }

    if (eleType != init) {
        return context.logErrorT("Initial type not matches", init_->getLoc());
    }

    //TODO verify array difenty type like
    //	type arrtype1 = array of int
    //	type arrtype2 = array of int
    //
    //	var arr1: arrtype1 := arrtype2 [10] of 0
    return type_;
}

llvm::FunctionType *Prototype::traverse(vector<VarDec *> &variableTable,
                                        CodeGenContext &context) {
    std::vector<llvm::Type *> args;
    auto linkType = llvm::PointerType::getUnqual(context.staticLink.front());
    args.push_back(linkType);
    frame = llvm::StructType::create(context.context, name_.getName() + "Frame");
    staticLink_ = new VarDec(getLoc(),
                             Identifier(getLoc(),
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
        resultType_ = context.typeOf(getLoc(), result_.getName());
    }

    if (!resultType_) {
        return nullptr;
    }
    auto functionType = llvm::FunctionType::get(resultType_, args, false);
    function_ = llvm::Function::Create(functionType, llvm::Function::InternalLinkage,
                                       name_.getName(), context.module.get());
    return functionType;
}

llvm::Type *FunctionDec::traverse(vector<VarDec *> &, CodeGenContext &context) {
    if (context.functions.lookupOne(name_.getName())) {
        return context.logErrorT("Function "
                                 + name_.getName() +
                                 " is already defined in same scope.", name_.getLoc());
    }

    context.valueDecs.enter();
    level_ = ++context.currentLevel;

    auto proto = proto_->traverse(variableTable_, context);
    if (!proto) {
        return nullptr;
    }

    context.staticLink.push_front(proto_->getFrame());
    context.functions.push(name_.getName(), proto_->getFunction());

    auto body = body_->traverse(variableTable_, context);
    context.staticLink.pop_front();
    if (!body) {
        return nullptr;
    }

    context.valueDecs.exit();
    --context.currentLevel;

    auto retType = proto_->getResultType();

    if (!body->isVoidTy() && retType->isVoidTy()) {
        return context.logErrorT("Procedure can not returns a value", getLoc());
    }

    if (!retType->isVoidTy() && retType != body) {
        return context.logErrorT("Function return type not match",
                                 proto_->getResult().getName().empty()
                                 ? getLoc()
                                 : proto_->getResult().getLoc());
    }

    return context.voidType;
}

llvm::Type *SimpleVar::traverse(vector<VarDec *> &, CodeGenContext &context) {
    auto type = context.valueDecs[name_.getName()];

    if (!type) {
        return context.logErrorT(name_.getName()
                                 + " is not defined", name_.getLoc());
    }

    return type->getType();
}

llvm::Type *VarDec::traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) {
    if (context.valueDecs.lookupOne(name_.getName())) {
        return context.logErrorT("Var: "
                                 + name_.getName()
                                 + " is already defined in same scope.",
                                 name_.getLoc());
    }

    offset_ = variableTable.size();
    level_ = context.currentLevel;
    variableTable.push_back(this);
    auto init = init_->traverse(variableTable, context);

    if (!typeName_) {
        if (init == context.nilType) {
            return context.logErrorT("Nil can only assign to record type", init_->getLoc());
        }

        type_ = init;
    } else {
        type_ = context.typeOf(typeName_->getName().getLoc(),
                               typeName_->getName().getName());;

        if (!context.isMatch(type_, init)) {
            return context.logErrorT("Type not match", typeName_->getLoc());
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
                                 name_.getLoc());
    }

    parentName.insert(name_.getName());
    auto type = context.typeOf(getLoc(), type_.getName(), parentName);
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
                                 name_.getLoc());
    }

    parentName.insert(name_.getName());

    auto type = context.typeOf(getLoc(), type_.getName(), parentName);
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
        auto type = context.typeOf(getLoc(), field->typeName_.getName(), parentName);
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

void Root::print(int depth) {
    root_->print(depth);
}

void AST::BinaryExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "BinaryExp: " << (char) op_ << endl;
    cout << tabs << AST::TAB << "Left: " << endl;
    left_->print(depth + 1);
    cout << tabs << AST::TAB << "Right: " << endl;
    right_->print(depth + 1);
    cout << tabs << ")" << endl;
}

void IntExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "IntExp: " << val_ << endl;
    cout << tabs << ")" << endl;
}

void StringExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "StringExp: " << val_ << endl;
    cout << tabs << ")" << endl;
}

void IfExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "IfExp:" << endl;

    cout << tabs << AST::TAB << "Test: " << endl;
    test_->print(depth + 1);

    cout << tabs << AST::TAB << "Then: " << endl;
    then_->print(depth + 1);

    if (else_) {
        cout << tabs << AST::TAB << "Else: " << endl;
        else_->print(depth + 1);
    }

    cout << tabs << ")" << endl;
}

void LetExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;

    cout << tabs << "LetExp: " << endl;

    cout << tabs << AST::TAB << "Decs: " << endl;
    for (auto &dec : decs_) {
        dec->print(depth + 1);
    }

    cout << tabs << AST::TAB << "Body: " << endl;
    body_->print(depth + 1);

    cout << tabs << ")" << endl;
}

void SequenceExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;

    cout << tabs << "SequenceExp:" << endl;

    cout << tabs << AST::TAB << "Exps: " << endl;
    for (auto &exp : exps_) {
        exp->print(depth + 1);
    }

    cout << tabs << ")" << endl;
}

void VarDec::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;

    cout << tabs << "VarDec:" << endl;

    cout << tabs << AST::TAB << "Name:" << endl;
    name_.print(depth + 1);

    if (typeName_) {
        cout << tabs << AST::TAB << "TypeName::" << endl;
        typeName_->print(depth + 1);
    }

    if (init_) {
        cout << tabs << AST::TAB << "Init:" << endl;
        init_->print(depth + 1);
    }

    cout << tabs << ")" << endl;
}

void Identifier::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "Identifier: " << name_ << endl;
    cout << tabs << ")" << endl;
}

void WhileExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "WhileExp:" << endl;

    cout << tabs << AST::TAB << "Test:" << endl;
    test_->print(depth + 1);

    cout << tabs << AST::TAB << "Body:" << endl;
    body_->print(depth + 1);

    cout << tabs << ")" << endl;
}

void DoWhileExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "DoWhileExp:" << endl;

    cout << tabs << AST::TAB << "Body:" << endl;
    body_->print(depth + 1);

    cout << tabs << AST::TAB << "Test:" << endl;
    test_->print(depth + 1);

    cout << tabs << ")" << endl;
}

void VarExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "VarExp: " << endl;

    cout << tabs << AST::TAB << "Var:" << endl;
    var_->print(depth + 1);

    cout << tabs << ")" << endl;
}

void SimpleVar::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "SimpleVar:" << endl;

    cout << tabs << AST::TAB << "Name:" << endl;
    name_.print(depth + 1);

    cout << tabs << ")" << endl;
}

void FieldVar::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "FieldVar:" << endl;

    cout << tabs << AST::TAB << "Var:" << endl;
    var_->print(depth + 1);

    cout << tabs << AST::TAB << "Field:" << endl;
    field_.print(depth + 1);

    cout << tabs << ")" << endl;
}

void SubscriptVar::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "SubscriptVar:" << endl;

    cout << tabs << AST::TAB << "Var:" << endl;
    var_->print(depth + 1);

    cout << tabs << AST::TAB << "Exp:" << endl;
    exp_->print(depth + 1);

    cout << tabs << ")" << endl;
}

void ForExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "ForExp:" << endl;

    cout << tabs << AST::TAB << "Var:" << endl;
    var_.print(depth + 1);

    cout << tabs << AST::TAB << "Low:" << endl;
    low_->print(depth + 1);

    cout << tabs << AST::TAB << "High:" << endl;
    high_->print(depth + 1);

    cout << tabs << AST::TAB << "Body:" << endl;
    body_->print(depth + 1);

    cout << tabs << ")" << endl;
}

void AST::BreakExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "BreakExp:" << endl;

    cout << tabs << ")" << endl;
}

void ArrayExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "ArrayExp:" << endl;

    cout << tabs << AST::TAB << "TypeName:" << endl;
    typeName_->print(depth + 1);

    cout << tabs << AST::TAB << "Size:"s << endl;
    size_->print(depth + 1);

    if (init_) {
        cout << tabs << AST::TAB << "Init:" << endl;
        init_->print(depth + 1);
    }

    cout << tabs << ")" << endl;
}

void NameType::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "NameType:" << endl;

    cout << tabs << AST::TAB << "Type:" << endl;
    type_.print(depth + 1);

    cout << tabs << ")" << endl;
}

void RecordType::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "RecordType:" << endl;

    cout << tabs << AST::TAB << "Name:" << endl;
    name_.print(depth + 1);

    cout << tabs << AST::TAB << "Fields: " << endl;
    for (auto &field : fields_) {
        field->print(depth + 1);
    }

    cout << tabs << ")" << endl;
}

void ArrayType::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "ArrayType:" << endl;

    cout << tabs << AST::TAB << "Name:" << endl;
    name_.print(depth + 1);

    cout << tabs << AST::TAB << "Type:" << endl;
    type_.print(depth + 1);

    cout << tabs << ")" << endl;
}

void TypeDec::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "TypeDec:" << endl;

    cout << tabs << AST::TAB << "Name:" << endl;
    name_.print(depth + 1);

    cout << tabs << AST::TAB << "Type:" << endl;
    type_->print(depth + 1);

    cout << tabs << ")" << endl;
}

void NilExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "NilExp:" << endl;

    cout << tabs << ")" << endl;
}

void CallExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "CallExp:" << endl;

    cout << tabs << AST::TAB << "Func:" << endl;
    func_.print(depth + 1);

    cout << tabs << AST::TAB << "Args: " << endl;
    for (auto &arg : args_) {
        arg->print(depth + 1);
    }

    cout << tabs << ")" << endl;
}

void Field::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "Field:" << endl;

    cout << tabs << AST::TAB << "Name:" << endl;
    name_.print(depth + 1);

    cout << tabs << AST::TAB << "TypeName:" << endl;
    typeName_.print(depth + 1);

    if (varDec_) {
        cout << tabs << AST::TAB << "VarDec:" << endl;
        varDec_->print(depth + 1);
    }

    cout << tabs << ")" << endl;
}

void FieldExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "FieldExp:" << endl;

    cout << tabs << AST::TAB << "Name:" << endl;
    name_.print(depth + 1);

    cout << tabs << AST::TAB << "Exp:" << endl;
    exp_->print(depth + 1);

    cout << tabs << ")" << endl;
}

void RecordExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "RecordExp:" << endl;

    cout << tabs << AST::TAB << "TypeName:" << endl;
    typeName_->print(depth + 1);

    cout << tabs << AST::TAB << "FieldExps: " << endl;
    for (auto &exp : fieldExps_) {
        exp->print(depth + 1);
    }

    cout << tabs << ")" << endl;
}

void AssignExp::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "AssignExp:" << endl;

    cout << tabs << AST::TAB << "Var:" << endl;
    var_->print(depth + 1);

    cout << tabs << AST::TAB << "Exp:" << endl;
    exp_->print(depth + 1);

    cout << tabs << ")" << endl;
}

void AST::Prototype::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "Prototype:" << endl;

    cout << tabs << AST::TAB << "Name:" << endl;
    name_.print(depth + 1);

    cout << tabs << AST::TAB << "Params: " << endl;
    for (auto &param : params_) {
        param->print(depth + 1);
    }

    cout << tabs << AST::TAB << "Result:" << endl;
    result_.print(depth + 1);

    cout << tabs << ")" << endl;
}

void FunctionDec::print(int depth) {
    string tabs = getTabs(depth);

    cout << tabs << "(" << endl;
    cout << tabs << "FunctionDec:" << endl;

    cout << tabs << AST::TAB << "Proto:" << endl;
    proto_->print(depth + 1);

    cout << tabs << AST::TAB << "Body:" << endl;
    body_->print(depth + 1);

    cout << tabs << ")" << endl;
}

llvm::Type *FunctionDec::computeHeaders(vector<VarDec *> &variableTable,
                                        CodeGenContext &context) {
    if (context.functions.lookupOne(name_.getName())) {
        return context.logErrorT("Function "
                                 + name_.getName() +
                                 " is already defined in same scope.", name_.getLoc());
    }

    context.valueDecs.enter();
    level_ = ++context.currentLevel;

    auto proto = proto_->traverse(variableTable_, context);
    if (!proto) {
        return nullptr;
    }

    context.staticLink.push_front(proto_->getFrame());
    context.functions.push(name_.getName(), proto_->getFunction());

    auto body = body_->traverse(variableTable_, context);
    context.staticLink.pop_front();
    if (!body) {
        return nullptr;
    }

    context.valueDecs.exit();
    --context.currentLevel;

    auto retType = proto_->getResultType();

    if (!body->isVoidTy() && retType->isVoidTy()) {
        return context.logErrorT("Procedure can not returns a value", getLoc());
    }

    if (!retType->isVoidTy() && retType != body) {
        return context.logErrorT("Function return type not match",
                                 proto_->getResult().getName().empty()
                                 ? getLoc()
                                 : proto_->getResult().getLoc());
    }

    return context.voidType;
}

llvm::Type *VarDec::computeHeaders(vector<VarDec *> &variableTable, CodeGenContext &context) {
    return nullptr;
}

llvm::Type *TypeDec::computeHeaders(vector<VarDec *> &variableTable, CodeGenContext &context) {
    return nullptr;
}

#include "codegencontext.hpp"
#include <iostream>

CodeGenContext::CodeGenContext() {}

void CodeGenContext::intrinsic() {
    functions["print"] = createIntrinsicFunction("print", {stringType}, voidType);
    functions["printd"] = createIntrinsicFunction("printd", {intType}, voidType);
    functions["flush"] = createIntrinsicFunction("flush", {}, voidType);
    functions["getchar"] = createIntrinsicFunction("getchar_", {}, stringType);
    functions["ord"] = createIntrinsicFunction("ord", {stringType}, intType);
    functions["chr"] = createIntrinsicFunction("chr", {intType}, stringType);
    functions["size"] = createIntrinsicFunction("size", {stringType}, intType);
    functions["substring"] = createIntrinsicFunction(
            "substring", {stringType, intType, intType}, stringType);
    functions["concat"] =
            createIntrinsicFunction("concat", {stringType, stringType}, stringType);
    functions["not"] = createIntrinsicFunction("not_", {intType}, intType);
    functions["exit"] = createIntrinsicFunction("exit_", {intType}, voidType);
}

llvm::Function *CodeGenContext::createIntrinsicFunction(
        std::string const &name, std::vector<llvm::Type *> const &args,
        llvm::Type *retType) {
    auto functionType = llvm::FunctionType::get(retType, args, false);
    auto function = llvm::Function::Create(functionType,
                                           llvm::Function::ExternalLinkage,
                                           name, module.get());
    functions.push(name, function);
    return function;
}

llvm::Value *CodeGenContext::strcmp(llvm::Value *a, llvm::Value *b) {
    return builder.CreateCall(strCmpFunction,
                              std::vector<llvm::Value *>{a, b},
                              "strcmp");
}

llvm::Value *CodeGenContext::checkStore(llvm::Value *val, llvm::Value *ptr) {
    val = convertNil(val, ptr);
    if (!val) {
        return nullptr;
    }

    return builder.CreateStore(val, ptr);
}

llvm::Value *CodeGenContext::convertNil(llvm::Value *val, llvm::Value *ptr) {
    if (isNil(val->getType())) {
        auto type = ptr->getType();
        if (isRecord(type)) {
            return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type));
        } else {
            //TODO verify codegen of rec asigned with nil
            return logErrorV("Nil can only assign to record type");
        }
    }

    return val;
}

llvm::Value *CodeGenContext::logErrorV(std::string const &msg) {
    hasError = true;
    std::cerr << msg << std::endl;
    return nullptr;
}

llvm::AllocaInst *CodeGenContext::createEntryBlockAlloca(llvm::Function *function,
                                                         llvm::Type *type,
                                                         const std::string &name,
                                                         llvm::Value *size) {
    llvm::IRBuilder<> TmpB(&function->getEntryBlock(),
                           function->getEntryBlock().begin());
    return TmpB.CreateAlloca(type, size, name);
}

llvm::Type *CodeGenContext::getElementType(llvm::Type *type) {
    return llvm::cast<llvm::PointerType>(type)->getElementType();
}

bool CodeGenContext::isNil(llvm::Type *exp) {
    return exp == nilType;
}

bool CodeGenContext::isRecord(llvm::Type *exp) {
    return exp->isPointerTy() && getElementType(exp)->isStructTy();
}

bool CodeGenContext::isMatch(llvm::Type *a, llvm::Type *b) {
    if (a == b) {
        return true;
    }

    if (!a || !b) {
        return false;
    }
    if (isNil(a)) {
        if (isRecord(b)) {
            return true;
        } else {
            return false;
        }
    }
    if (isNil(b)) {
        return isRecord(a);
    }
    if (isRecord(a) && isRecord(b)) {
        return llvm::cast<llvm::StructType>(getElementType(a))
                ->isLayoutIdentical(llvm::cast<llvm::StructType>(getElementType(b)));
    }

    return false;
}

llvm::Type *CodeGenContext::logErrorT(std::string const &msg,
                                      AST::Location const &loc) {
    hasError = true;
    std::cerr << loc.getFirstLine() << ":" << loc.getFirstColumn() << ": "
              << "Error: " << msg << std::endl;
    return nullptr;
}

llvm::Type *CodeGenContext::typeOf(const AST::Location &loc,
                                   std::string const &name,
                                   std::set<std::string> &parentName) {
    if (auto type = types[name]) {
        return type;
    }

    auto typeDec = typeDecs[name];
    if (!typeDec) {
        return logErrorT(name + " is not a type", loc);
    }

    return typeDec->traverse(parentName, *this);
}

llvm::Type *CodeGenContext::typeOf(const AST::Location &loc, const std::string &name) {
    std::set<std::string> parentName;

    return typeOf(loc, name, parentName);
}

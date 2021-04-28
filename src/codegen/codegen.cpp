#include "utils/codegencontext.hpp"
#include <iostream>
#include <stack>
#include <tuple>
#include "ast/ast.hpp"

llvm::Value *AST::Root::codegen(CodeGenContext &context) {
    // clear context.builder and context;
    llvm::legacy::PassManager pm;
//    pm.add(llvm::createPrintModulePass(llvm::outs())); // dont print IR to stdio

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    context.module->setTargetTriple(targetTriple);

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);

    // Print an error and exit if we couldn't find the requested target.
    // This generally occurs if we've forgotten to initialise the
    // TargetRegistry or we have a bogus target triple.
    if (!target) {
        llvm::errs() << error;
        return nullptr;
    }

    auto CPU = "generic";
    auto features = "";

    llvm::TargetOptions opt;
    auto RM = llvm::Optional<llvm::Reloc::Model>();
    auto targetMachine = target->createTargetMachine(targetTriple, CPU, features, opt, RM);

    context.module->setDataLayout(targetMachine->createDataLayout());
    std::vector<llvm::Type *> args;
    auto mainProto = llvm::FunctionType::get(llvm::Type::getInt64Ty(context.context),
                                             llvm::makeArrayRef(args),
                                             false);
    auto mainFunction = llvm::Function::Create(mainProto,
                                               llvm::GlobalValue::ExternalLinkage,
                                               "main",
                                               context.module.get());
    context.staticLink.push_front(llvm::StructType::create(context.context,
                                                           "main"));

    auto block = llvm::BasicBlock::Create(context.context, "entry", mainFunction);
    context.types["int"] = context.intType;
    context.types["string"] = context.stringType;
//    context.intrinsic();
    traverse(mainVariableTable_, context);

    if (context.hasError) {
        return nullptr;
    }

    context.valueDecs.reset();
    context.functionDecs.reset();
    context.builder.SetInsertPoint(block);
    std::vector<llvm::Type *> localVar;

    for (auto &var : mainVariableTable_) {
        localVar.push_back(var->getType());
        context.valueDecs.push(var->getName(), var);
    }

    context.staticLink.front()->setBody(localVar);
    context.currentFrame = context.createEntryBlockAlloca(mainFunction,
                                                          context.staticLink.front(),
                                                          "mainframe");
    context.currentLevel = 0;
    root_->codegen(context);
    context.builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.context),
                                                     llvm::APInt(64, 0)));

    if (llvm::verifyFunction(*mainFunction, &llvm::errs())) {
        return context.logErrorV("Generate fail");
    }

    // llvm::ReturnInst::Create(context, block);
//    std::cout << "Code is generated." << std::endl;

    if (!context.outputFileI.empty()) {
        std::error_code EC;
//        llvm::raw_fd_ostream dest(context.outputFileO, EC, llvm::sys::fs::F_None);
//        if (EC) {
//            llvm::errs() << "Could not open file: " << EC.message();
//            return nullptr;
//        }
//
//        // llvm::legacy::PassManager pass;
//        auto fileType = llvm::CGFT_ObjectFile;
//
//        if (targetMachine->addPassesToEmitFile(pm, dest, nullptr, fileType)) {
//            llvm::errs() << "TheTargetMachine can't emit a file of this type";
//            return nullptr;
//        }
//
//        pm.run(*context.module);
//        // pass.run(*module);
//        dest.flush();

//    context.module->print(llvm::errs(), nullptr);

        llvm::raw_fd_ostream dest_txt(context.outputFileI, EC, llvm::sys::fs::F_None);
        dest_txt << *context.module;
        dest_txt.flush();

//        llvm::outs() << "Wrote " << context.outputFileO << "\n";
    }

    return nullptr;
}

llvm::Value *AST::SimpleVar::codegen(CodeGenContext &context) {
    auto var = context.valueDecs[name_.getName()];
    if (!var) {
        return context.logErrorV("Unknown variable name " + name_.getName());
    }

    return var->read(context);
}

llvm::Value *AST::IntExp::codegen(CodeGenContext &context) {
    return llvm::ConstantInt::get(context.context, llvm::APInt(64, val_));
}

llvm::Value *AST::BreakExp::codegen(CodeGenContext &context) {
}

llvm::Value *AST::ForExp::codegen(CodeGenContext &context) {
}

llvm::Value *AST::SequenceExp::codegen(CodeGenContext &context) {
    llvm::Value *last = nullptr;

    for (auto &exp : exps_) {
        last = exp->codegen(context);
    }

    return last;
}

llvm::Value *AST::LetExp::codegen(CodeGenContext &context) {
    context.valueDecs.enter();
    context.functionDecs.enter();

    for (auto &dec : decs_) {
        dec->codegen(context);
    }

    auto result = body_->codegen(context);

    context.functionDecs.exit();
    context.valueDecs.exit();

    return result;
}

llvm::Value *AST::NilExp::codegen(CodeGenContext &context) {
}

llvm::Value *AST::VarExp::codegen(CodeGenContext &context) {
    auto var = var_->codegen(context);
    if (!var) {
        return nullptr;
    }

    return context.builder.CreateLoad(var, var->getName());
}

llvm::Value *AST::AssignExp::codegen(CodeGenContext &context) {
    auto var = var_->codegen(context);
    if (!var) {
        return nullptr;
    }

    auto exp = exp_->codegen(context);
    if (!exp) {
        return nullptr;
    }

    context.checkStore(exp, var);

    return exp;  // var is a pointer, should not return
}

llvm::Value *AST::IfExp::codegen(CodeGenContext &context) {
}

llvm::Value *generateWhileLoop(CodeGenContext &context,
                               std::unique_ptr<AST::Exp> &test,
                               std::unique_ptr<AST::Exp> &body) {
    auto function = context.builder.GetInsertBlock()->getParent();
    auto testBB = llvm::BasicBlock::Create(context.context, "test__", function);
    auto loopBB = llvm::BasicBlock::Create(context.context, "loop", function);
    auto nextBB = llvm::BasicBlock::Create(context.context, "next", function);
    auto afterBB = llvm::BasicBlock::Create(context.context, "after", function);

    context.loopStack.push({nextBB, afterBB});
    context.builder.CreateBr(testBB);
    context.builder.SetInsertPoint(testBB);

    auto _test = test->codegen(context);
    if (!_test) {
        return nullptr;
    }

    auto EndCond = context.builder.CreateICmpEQ(_test, context.zero, "loopcond");
    context.builder.CreateCondBr(EndCond, afterBB, loopBB);

    context.builder.SetInsertPoint(loopBB);
    if (!body->codegen(context)) {
        return nullptr;
    }

    context.builder.CreateBr(nextBB);
    context.builder.SetInsertPoint(nextBB);

    context.builder.CreateBr(testBB);
    context.builder.SetInsertPoint(afterBB);

    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(context.context));
}

llvm::Value *AST::WhileExp::codegen(CodeGenContext &context) {
    return generateWhileLoop(context, test_, body_);
}

llvm::Value *AST::DoWhileExp::codegen(CodeGenContext &context) {
    if (!body_->codegen(context)) {
        return nullptr;
    }

    return generateWhileLoop(context, test_, body_);
}

llvm::Value *AST::CallExp::codegen(CodeGenContext &context) {
    auto callee = context.functionDecs[func_.getName()];
    llvm::Function *function;
    llvm::Function::LinkageTypes type;

    size_t level = 0u;
    if (!callee) {
        function = context.functions[func_.getName()];
        if (!function) {
            return context.logErrorV("Function "
                                     + func_.getName()
                                     + " not found");
        }
        type = llvm::Function::ExternalLinkage;
    } else {
        function = callee->getProto().getFunction();
        type = function->getLinkage();
        level = callee->getLevel();
    }

    // If argument mismatch error.
    std::vector<llvm::Value *> args;
    if (type == llvm::Function::InternalLinkage) {
        size_t currentLevel = context.currentLevel;
        auto staticLink = context.staticLink.begin();
        llvm::Value *value = context.currentFrame;
        while (currentLevel-- >= level) {
            value = context.builder.CreateGEP(llvm::PointerType::getUnqual(*++staticLink),
                                              value, context.zero, "staticLink");
            value = context.builder.CreateLoad(value, "frame");
        }
        args.push_back(value);
    }

    for (size_t i = 0u; i != args_.size(); ++i) {
        args.push_back(args_[i]->codegen(context));
        if (!args.back()) {
            return nullptr;
        }
    }

    if (function->getFunctionType()->getReturnType()->isVoidTy()) {
        return context.builder.CreateCall(function, args);
    } else {
        return context.builder.CreateCall(function, args, "calltmp");
    }
}

llvm::Value *AST::ArrayExp::codegen(CodeGenContext &context) {
}

llvm::Value *AST::SubscriptVar::codegen(CodeGenContext &context) {
    // auto var = var_->codegen(context);
    // auto exp = exp_->codegen(context);
    // if (!var) return nullptr;
    // var = context.builder.CreateLoad(var, "arrayPtr");
    // return context.builder.CreateGEP(type_, var, exp, "ptr");
}

llvm::Value *AST::FieldVar::codegen(CodeGenContext &context) {
    // auto var = var_->codegen(context);
    // if (!var) return nullptr;
    // var = context.builder.CreateLoad(var, "structPtr");
    // auto idx = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.context),
    //                                   llvm::APInt(64, idx_));
    // return context.builder.CreateGEP(type_, var, idx, "ptr");
}

llvm::Value *AST::FieldExp::codegen(CodeGenContext &context) {
    // return exp_->codegen(context);
}

llvm::Value *AST::RecordExp::codegen(CodeGenContext &context) {
    // context.builder.GetInsertBlock()->getParent();
    // if (!type_) return nullptr;
    // // auto var = createEntryBlockAlloca(function, type, "record");
    // auto eleType = context.getElementType(type_);
    // auto size = context.module->getDataLayout().getTypeAllocSize(eleType);
    // llvm::Value *var = context.builder.CreateCall(
    //     context.allocaRecordFunction,
    //     llvm::ConstantInt::get(context.intType, llvm::APInt(64, size)), "alloca");
    // var = context.builder.CreateBitCast(var, type_, "record");
    // size_t idx = 0u;
    // for (auto &field : fieldExps_) {
    //   auto exp = field->codegen(context);
    //   if (!exp) return nullptr;
    //   if (!field->type_) return nullptr;
    //   auto elementPtr = context.builder.CreateGEP(
    //       field->type_, var,
    //       llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.context),
    //                              llvm::APInt(64, idx)),
    //       "elementPtr");
    //   context.checkStore(exp, elementPtr);
    //   // context.builder.CreateStore(exp, elementPtr);
    //   ++idx;
    // }
    // return var;
}

llvm::Value *AST::StringExp::codegen(CodeGenContext &context) {
    return context.builder.CreateGlobalStringPtr(val_, "str");
}

llvm::Function *AST::Prototype::codegen(CodeGenContext &) {
}

// TODO: Static link
llvm::Value *AST::FunctionDec::codegen(CodeGenContext &context) {
}

llvm::Value *AST::VarDec::codegen(CodeGenContext &context) {
    // llvm::Function *function = context.builder.GetInsertBlock()->getParent();
    auto init = init_->codegen(context);
    if (!init) {
        return nullptr;
    }
    // auto *variable = context.createEntryBlockAlloca(function, type_, name_);
    //  if (isNil(init)) {
    //    if (!type->isStructTy()) {
    //      return context.logErrorVV("Nil can only assign to struct type");
    //    } else {
    //      init =
    //      llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(type));
    //    }
    //  }
    auto var = context.checkStore(init, read(context));
    // context.builder.CreateStore(init, variable);

    context.valueDecs.push(name_.getName(), this);

    return var;
}

llvm::Value *AST::VarDec::read(CodeGenContext &context) {
    size_t currentLevel = context.currentLevel;
    auto staticLink = context.staticLink.begin();
    llvm::Value *value = context.currentFrame;

    while (currentLevel-- > level_) {
        value = context.builder.CreateGEP(llvm::PointerType::getUnqual(*++staticLink),
                                          value, context.zero, "staticLink");
        value = context.builder.CreateLoad(value, "frame");
    }

    std::vector<llvm::Value *> indices(2);
    indices[0] = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.context),
                                        llvm::APInt(32, 0));
    indices[1] = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.context),
                                        llvm::APInt(32, offset_));

    return context.builder.CreateGEP(*staticLink, value, indices, name_.getName());
}

llvm::Value *AST::TypeDec::codegen(CodeGenContext &context) {
     return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(context.context));
}

llvm::Value *createAdd(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    return context.builder.CreateAdd(L, R, "addtmp");
}

llvm::Value *createSub(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    return context.builder.CreateSub(L, R, "subtmp");
}

llvm::Value *createMul(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    return context.builder.CreateMul(L, R, "multmp");
}

llvm::Value *createDiv(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    return context.builder.CreateFPToSI(
            context.builder.CreateFDiv(
                    context.builder.CreateSIToFP(
                            L, llvm::Type::getDoubleTy(context.context)),
                    context.builder.CreateSIToFP(
                            R, llvm::Type::getDoubleTy(context.context)),
                    "divftmp"),
            llvm::Type::getInt64Ty(context.context), "divtmp");
}

llvm::Value *createLTH(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    if (L->getType() == context.stringType) {
        L = context.strcmp(L, R);
        R = context.zero;
    }

    return context.builder
            .CreateZExt(context.builder
                                .CreateICmpSLT(L, R, "cmptmp"),
                        context.intType,
                        "cmptmp");
}

llvm::Value *createGTH(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    if (L->getType() == context.stringType) {
        L = context.strcmp(L, R);
        R = context.zero;
    }

    return context.builder
            .CreateZExt(context.builder
                                .CreateICmpSGT(L, R, "cmptmp"),
                        context.intType,
                        "cmptmp");
}

llvm::Value *createEQU(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    L = context.convertNil(L, R);
    R = context.convertNil(R, L);

    if (L->getType() == context.stringType) {
        L = context.strcmp(L, R);
        R = context.zero;
    }

    return context.builder
            .CreateZExt(context.builder
                                .CreateICmpEQ(L, R, "cmptmp"),
                        context.intType,
                        "cmptmp");
}

llvm::Value *createNEQU(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    L = context.convertNil(L, R);
    R = context.convertNil(R, L);

    if (L->getType() == context.stringType) {
        L = context.strcmp(L, R);
        R = context.zero;
    }

    if (L->getType() == context.stringType) {
        return context.strcmp(L, R);
    }

    return context.builder
            .CreateZExt(context.builder
                                .CreateICmpNE(L, R, "cmptmp"),
                        context.intType,
                        "cmptmp");
}

llvm::Value *createLEQ(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    if (L->getType() == context.stringType) {
        L = context.strcmp(L, R);
        R = context.zero;
    }

    return context.builder
            .CreateZExt(context.builder
                                .CreateICmpSLE(L, R, "cmptmp"),
                        context.intType,
                        "cmptmp");
}

llvm::Value *createGEQ(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    if (L->getType() == context.stringType) {
        L = context.strcmp(L, R);
        R = context.zero;
    }

    return context.builder
            .CreateZExt(context.builder
                                .CreateICmpSGE(L, R, "cmptmp"),
                        context.intType,
                        "cmptmp");
}

llvm::Value *createAnd(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    return context.builder.CreateAnd(L, R, "andtmp");
}

llvm::Value *createOr(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    return context.builder.CreateOr(L, R, "ortmp");
}

llvm::Value *createXor(CodeGenContext &context, llvm::Value *L, llvm::Value *R) {
    return context.builder.CreateXor(L, R, "xortmp");
}

llvm::Value *AST::BinaryExp::codegen(CodeGenContext &context) {
    auto L = left_->codegen(context);
    auto R = right_->codegen(context);

    if (!L || !R) {
        return nullptr;
    }

    // TODO: check for nil
    switch (op_) {
        case ADD:
            return createAdd(context, L, R);
        case SUB:
            return createSub(context, L, R);
        case MUL:
            return createMul(context, L, R);
        case DIV:
            return createDiv(context, L, R);
        case LTH:
            return createLTH(context, L, R);
        case GTH:
            return createGTH(context, L, R);
        case EQU:
            return createEQU(context, L, R);
        case NEQU:
            return createNEQU(context, L, R);
        case LEQU:
            return createLEQ(context, L, R);
        case GEQU:
            return createGEQ(context, L, R);
        case AND_:
            return createAnd(context, L, R);
        case OR_:
            return createOr(context, L, R);
        case XOR:
            return createXor(context, L, R);
        default:
            return nullptr;
    }
}

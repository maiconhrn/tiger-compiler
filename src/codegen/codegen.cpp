#include "utils/codegencontext.hpp"
#include <iostream>
#include <stack>
#include <tuple>
#include "ast/ast.hpp"

llvm::Value *AST::Root::codegen(CodeGenContext &context) {
    llvm::legacy::PassManager pm;

    root_->codegen(context);
    context.builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.context),
                                                     llvm::APInt(64, 0)));

    if (llvm::verifyFunction(*context.mainFunction, &llvm::errs())) {
        return context.logErrorV("Generate fail");
    }

    if (!context.outputFileI.empty()) {
        std::error_code EC;

        llvm::raw_fd_ostream dest_txt(context.outputFileI, EC, llvm::sys::fs::F_None);
        dest_txt << *context.module;
        dest_txt.flush();
    }

    std::error_code EC;
    llvm::raw_fd_ostream dest(context.outputFileO, EC, llvm::sys::fs::F_None);

    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message();
        return nullptr;
    }

    auto fileType = llvm::CGFT_ObjectFile;
    if (context.targetMachine->addPassesToEmitFile(pm, dest, nullptr, fileType)) {
        llvm::errs() << "TheTargetMachine can't emit a file of this type";
        return nullptr;
    }

    pm.run(*context.module);
    dest.flush();

    return nullptr;
}

llvm::Value *AST::SimpleVar::codegen(CodeGenContext &context) {
    auto var = context.namedValues[name_.getName()];

    if (!var) {
        return context.logErrorV("Unknown variable name " + name_.getName());
    }

    return var;
}

llvm::Value *AST::IntExp::codegen(CodeGenContext &context) {
    return llvm::ConstantInt::get(context.context, llvm::APInt(64, val_));
}

llvm::Value *AST::BreakExp::codegen(CodeGenContext &context) {
    context.builder.CreateBr(std::get<1>(context.loopStack.top()));

    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(context.context));
}

llvm::Value *AST::ForExp::codegen(CodeGenContext &context) {
    context.namedValues.enter();
    context.valueDecs.enter();

    auto *function = context.builder.GetInsertBlock()->getParent();

    auto *alloca = context.createEntryBlockAlloca(function, varDec_->getType(),
                                                  varDec_->getName());
    auto low = low_->codegen(context);
    if (!low) {
        return nullptr;
    }
    if (!low->getType()->isIntegerTy()) {
        return context.logErrorV("loop lower bound should be integer");
    }

    context.builder.CreateStore(low, alloca);

    auto high = high_->codegen(context);
    if (!high) {
        return nullptr;
    }
    if (!high->getType()->isIntegerTy()) {
        return context.logErrorV("loop higher bound should be integer");
    }

    auto testBB = llvm::BasicBlock::Create(context.context, "test", function);
    auto loopBB = llvm::BasicBlock::Create(context.context, "loop", function);
    auto nextBB = llvm::BasicBlock::Create(context.context, "next", function);
    auto afterBB = llvm::BasicBlock::Create(context.context, "after", function);

    context.loopStack.push({nextBB, afterBB});
    context.builder.CreateBr(testBB);
    context.builder.SetInsertPoint(testBB);

    auto EndCond = context.builder.CreateICmpSLE(context.builder.CreateLoad((llvm::Value *) alloca,
                                                                            var_.getName()), high,
                                                 "loopcond");

    context.builder.CreateCondBr(EndCond, loopBB, afterBB);

    context.builder.SetInsertPoint(loopBB);

    auto oldVal = context.valueDecs[var_.getName()];
    if (oldVal) {
        context.valueDecs.popOne(var_.getName());
    }
    context.valueDecs.push(var_.getName(), varDec_);

    auto oldValN = context.namedValues[var_.getName()];
    if (oldValN) {
        context.namedValues.popOne(var_.getName());
    }
    context.namedValues.push(varDec_->getName(), alloca);

    if (!body_->codegen(context)) {
        return nullptr;
    }

    // goto next:
    context.builder.CreateBr(nextBB);

    // next:
    context.builder.SetInsertPoint(nextBB);

    auto nextVar = context.builder.CreateAdd(context.builder.CreateLoad((llvm::Value *) alloca,
                                                                        var_.getName()),
                                             llvm::ConstantInt::get(context.context,
                                                                    llvm::APInt(64, 1)),
                                             "nextvar");
    context.builder.CreateStore(nextVar, alloca);

    context.builder.CreateBr(testBB);

    // after:
    context.builder.SetInsertPoint(afterBB);

    if (oldVal && oldValN) {
        context.valueDecs[var_.getName()] = oldVal;
        context.namedValues[var_.getName()] = oldValN;
    } else {
        context.valueDecs.popOne(var_.getName());
        context.namedValues.popOne(var_.getName());
    }

    context.loopStack.pop();

    context.valueDecs.exit();
    context.namedValues.exit();

    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(context.context));
}

llvm::Value *AST::SequenceExp::codegen(CodeGenContext &context) {
    llvm::Value *last = llvm::Constant::getNullValue(context.nilType);

    for (auto &exp : exps_) {
        last = exp->codegen(context);
        if (!last) {
            return nullptr;
        }
    }

    return last;
}

llvm::Value *AST::LetExp::codegen(CodeGenContext &context) {
    context.valueDecs.enter();
    context.namedValues.enter();
    context.functionDecs.enter();

    for (auto &dec : decs_) {
        dec->computeHeaderCodegen(context);
    }

    for (auto &dec : decs_) {
        if (!dec->codegen(context)) {
            return nullptr;
        }
    }

    auto result = body_->codegen(context);

    context.functionDecs.exit();
    context.namedValues.exit();
    context.valueDecs.exit();

    return result;
}

llvm::Value *AST::NilExp::codegen(CodeGenContext &context) {
    return llvm::ConstantPointerNull::get(context.nilType);
}

llvm::Value *AST::VarExp::codegen(CodeGenContext &context) {
    auto var = var_->codegen(context);
    if (!var) {
        return nullptr;
    }

    return context.builder.CreateLoad(((llvm::AllocaInst *) var)->getAllocatedType(),
                                      var, var->getName());
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

    return exp;
}

llvm::Value *AST::IfExp::codegen(CodeGenContext &context) {
    auto test = test_->codegen(context);
    if (!test) {
        return nullptr;
    }

    test = context.builder.CreateICmpNE(test, context.zero, "iftest");
    auto function = context.builder.GetInsertBlock()->getParent();

    auto thenBB = llvm::BasicBlock::Create(context.context, "then", function);
    auto elseBB = llvm::BasicBlock::Create(context.context, "else");
    auto mergeBB = llvm::BasicBlock::Create(context.context, "ifcont");

    context.builder.CreateCondBr(test, thenBB, elseBB);

    context.builder.SetInsertPoint(thenBB);

    auto then = then_->codegen(context);
    if (!then) {
        return nullptr;
    }

    context.builder.CreateBr(mergeBB);

    thenBB = context.builder.GetInsertBlock();

    function->getBasicBlockList().push_back(elseBB);
    context.builder.SetInsertPoint(elseBB);

    llvm::Value *elsee;
    if (else_) {
        elsee = else_->codegen(context);
        if (!elsee) {
            return nullptr;
        }
    }

    context.builder.CreateBr(mergeBB);
    elseBB = context.builder.GetInsertBlock();

    function->getBasicBlockList().push_back(mergeBB);
    context.builder.SetInsertPoint(mergeBB);

    if (else_ && !then->getType()->isVoidTy() && !elsee->getType()->isVoidTy()) {
        auto PN = context.builder.CreatePHI(then->getType(), 2, "iftmp");
        then = context.convertNil(then, elsee);
        elsee = context.convertNil(elsee, then);
        PN->addIncoming(then, thenBB);
        PN->addIncoming(elsee, elseBB);

        return PN;
    }

    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(context.context));
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

    if (!callee) {
        function = context.functions[func_.getName()];
        if (!function) {
            return context.logErrorV("Function "
                                     + func_.getName()
                                     + " not found");
        }
    } else {
        function = callee->getProto().getFunction();
    }

    std::vector<llvm::Value *> args;
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
    auto function = context.builder.GetInsertBlock()->getParent();
    auto eleType = context.getElementType(type_);
    auto size = size_->codegen(context);
    auto init = init_->codegen(context);
    auto eleSize = context.module->getDataLayout().getTypeAllocSize(eleType);
    llvm::Value *arrayPtr = context.builder
            .CreateCall(context.allocaArrayFunction,
                        std::vector<llvm::Value *>{size,
                                                   llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.context),
                                                                          llvm::APInt(64,
                                                                                      eleSize))},
                        "alloca");
    arrayPtr = context.builder.CreateBitCast(arrayPtr, type_, "array");

    auto zero = llvm::ConstantInt::get(context.context, llvm::APInt(64, 0, true));

    std::string indexName = "index";
    auto indexPtr = context.createEntryBlockAlloca(
            function, llvm::Type::getInt64Ty(context.context), indexName);
    // before loop:
    context.builder.CreateStore(zero, indexPtr);

    auto testBB = llvm::BasicBlock::Create(context.context, "test", function);
    auto loopBB = llvm::BasicBlock::Create(context.context, "loop", function);
    auto nextBB = llvm::BasicBlock::Create(context.context, "next", function);
    auto afterBB = llvm::BasicBlock::Create(context.context, "after", function);

    context.builder.CreateBr(testBB);

    context.builder.SetInsertPoint(testBB);

    auto index = context.builder.CreateLoad(indexPtr, indexName);
    auto EndCond = context.builder.CreateICmpSLT(index, size, "loopcond");

    // goto after or loop
    context.builder.CreateCondBr(EndCond, loopBB, afterBB);

    context.builder.SetInsertPoint(loopBB);

    // loop:
    auto elePtr = context.builder.CreateGEP(eleType, arrayPtr, index, "elePtr");
    context.checkStore(init, elePtr);

    // goto next:
    context.builder.CreateBr(nextBB);

    // next:
    context.builder.SetInsertPoint(nextBB);

    auto nextVar = context.builder.CreateAdd(
            index, llvm::ConstantInt::get(context.context,
                                          llvm::APInt(64, 1)),
            "nextvar");
    context.builder.CreateStore(nextVar, indexPtr);

    context.builder.CreateBr(testBB);

    // after:
    context.builder.SetInsertPoint(afterBB);

    return arrayPtr;
}

llvm::Value *AST::SubscriptVar::codegen(CodeGenContext &context) {
    auto var = var_->codegen(context);
    auto exp = exp_->codegen(context);

    if (!var) {
        return nullptr;
    }

    var = context.builder.CreateLoad(var, "arrayPtr");

    return context.builder.CreateGEP(type_, var, exp, "ptr");
}

llvm::Value *AST::FieldVar::codegen(CodeGenContext &context) {
    auto var = var_->codegen(context);
    if (!var) {
        return nullptr;
    }

    var = context.builder.CreateLoad(var, "structPtr");

    auto idx = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.context),
                                      llvm::APInt(64, idx_));

    return context.builder.CreateGEP(type_, var, idx, "ptr");
}

llvm::Value *AST::FieldExp::codegen(CodeGenContext &context) {
    return exp_->codegen(context);
}

llvm::Value *AST::RecordExp::codegen(CodeGenContext &context) {
    llvm::Type *objType = context.module->getTypeByName(typeName_->getName().getName());

    context.builder.GetInsertBlock()->getParent();

    if (!objType) {
        return nullptr;
    }

    llvm::Value *objDummyPtr = context.builder.CreateConstGEP1_64(llvm::Constant::getNullValue(objType->getPointerTo()),
                                                                  1, "objsize");
    llvm::Value *objSize = context.builder.CreatePointerCast(objDummyPtr, llvm::Type::getInt64Ty(context.context));
    llvm::Value *objVoidPtr = context.builder.CreateCall(context.allocaRecordFunction, objSize);
    llvm::Value *obj = context.builder.CreatePointerCast(objVoidPtr, objType->getPointerTo());

    size_t idx = 0u;
    for (auto &field : fieldExps_) {
        auto exp = field->codegen(context);
        if (!exp) {
            return nullptr;
        }

        if (!field->type_) {
            return nullptr;
        }

        //TODO adjust assertion error:
        //tc: /usr/lib/llvm-10/include/llvm/IR/Instructions.h:927: static llvm::GetElementPtrInst* llvm::GetElementPtrInst::Create(llvm::Type*, llvm::Value*, llvm::ArrayRef<llvm::Value*>, const llvm::Twine&, llvm::Instruction*): Assertion `PointeeType == cast<PointerType>(Ptr->getType()->getScalarType())->getElementType()' failed.
        auto elementPtr = context.builder.CreateStructGEP(obj,
                                                          idx,
                                                          "elementPtr");
        context.checkStore(exp, elementPtr);
        ++idx;
    }

    return obj;
}

llvm::Value *AST::StringExp::codegen(CodeGenContext &context) {
    return context.builder.CreateGlobalStringPtr(val_, "str");
}

llvm::Function *AST::Prototype::codegen(CodeGenContext &) {
    size_t idx = 0u;
    for (auto &arg : function_->args()) {
        arg.setName(params_[idx++]->getName());
    }

    return function_;
}

llvm::Value *AST::FunctionDec::computeHeaderCodegen(CodeGenContext &context) {
    if (context.functions.lookupOne(name_.getName())
        && this->getConcreteType() == (context.lastDec ? context.lastDec->getConcreteType() : "")
        && this->getName() == context.lastDec->getName()) {
        return context.logErrorV("Function "
                                 + name_.getName() +
                                 " is already defined in same scope.");
    }

    auto function = proto_->codegen(context);
    if (!function) {
        return nullptr;
    }

    context.functionDecs.push(name_.getName(), this);
    context.lastDec = this;

    return function;
}

llvm::Value *AST::FunctionDec::codegen(CodeGenContext &context) {
    auto function = proto_->getFunction();

    auto oldBB = context.builder.GetInsertBlock();

    auto BB = llvm::BasicBlock::Create(context.context, "entry", function);
    context.builder.SetInsertPoint(BB);

    context.valueDecs.enter();
    context.namedValues.enter();
    ++context.currentLevel;

    size_t idx = 0;
    for (auto &arg : function->args()) {
        llvm::AllocaInst *alloca = context.createEntryBlockAlloca(function, arg.getType(), arg.getName());
        context.builder.CreateStore(&arg, alloca);

        context.namedValues.push(arg.getName(), alloca);
        context.valueDecs.push(arg.getName(), proto_->getParams()[idx++]->getVar());
    }

    if (auto retVal = body_->codegen(context)) {
        if (proto_->getResultType()->isVoidTy()) {
            context.builder.CreateRetVoid();
        } else {
            context.builder.CreateRet(retVal);
        }

        if (!llvm::verifyFunction(*function, &llvm::errs())) {
            context.valueDecs.exit();
            context.namedValues.exit();
            context.builder.SetInsertPoint(oldBB);
            --context.currentLevel;

            return function;
        }
    }

    context.valueDecs.exit();
    context.namedValues.exit();
    function->eraseFromParent();
    context.functionDecs.popOne(name_.getName());
    context.builder.SetInsertPoint(oldBB);
    --context.currentLevel;

    return context.logErrorV("Function " + name_.getName() + " genteration failed");
}

llvm::Value *AST::VarDec::computeHeaderCodegen(CodeGenContext &context) {
    context.lastDec = this;

    return nullptr;
}

llvm::Value *AST::VarDec::codegen(CodeGenContext &context) {
    auto value = context.namedValues.lookupOne(name_.getName());
    auto *function = context.builder.GetInsertBlock()->getParent();
    auto *alloca = value
                   ? value
                   : context.createEntryBlockAlloca(function, type_, getName());

    auto init = init_->codegen(context);
    if (!init) {
        return nullptr;
    }

    context.builder.CreateStore(init, alloca);

    context.namedValues.push(getName(), alloca);
    context.valueDecs.push(getName(), this);

    return alloca;
}

llvm::Value *AST::TypeDec::computeHeaderCodegen(CodeGenContext &context) {
    context.lastDec = this;

    return nullptr;
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

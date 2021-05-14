#ifndef AST_HPP
#define AST_HPP

#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include "utils/codegencontext.hpp"
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <iostream>

class CodeGenContext;
namespace AST {
    using llvm::Value;
    using std::move;
    using std::reverse;
    using std::set;
    using std::string;
    using std::unique_ptr;
    using std::vector;
    using std::cout;
    using std::cerr;
    using std::endl;

    const static string TAB = "  ";

    class VarDec;

    class Node {
        size_t depth_ = 0;

    public:
        virtual ~Node() = default;

        virtual Value *codegen(CodeGenContext &context) = 0;

        size_t &getDepth() {
            return depth_;
        }

        void setDepth(const size_t &depth) {
            depth_ = depth;
        }

        virtual llvm::Type *traverse(vector<VarDec *> &, CodeGenContext &) = 0;

        virtual void print(int depth) = 0;
    };

    class Location {
        int first_line_;
        int first_column_;

    public:
        Location(int first_line, int first_column) :
                first_line_(first_line), first_column_(first_column) {}

        virtual ~Location() = default;

        int getFirstLine() const {
            return first_line_;
        }

        int getFirstColumn() const {
            return first_column_;
        }
    };


    class Identifier : public Node {
        Location loc_;
        string name_;

    public:
        Identifier(Location loc, string name) :
                loc_(move(loc)), name_(name) {}

        virtual ~Identifier() = default;

        Location &getLoc() {
            return loc_;
        }

        string &getName() {
            return name_;
        }

        Value *codegen(CodeGenContext &context) override {
            return nullptr;
        }

        llvm::Type *traverse(vector<VarDec *> &vector, CodeGenContext &context) override {
            return nullptr;
        }

        void print(int depth) override;
    };

    class Var : public Node {
        Location loc_;

    public:
        Var(Location loc) : loc_(move(loc)) {}

        Location &getLoc() {
            return loc_;
        }

        void print(int depth) override {
            std::cerr << "Print not implented" << endl;
        }
    };

    class Exp : public Node {
        Location loc_;

    public:
        Exp(Location loc) : loc_(move(loc)) {}

        Location &getLoc() {
            return loc_;
        }

        void print(int depth) override {
            std::cerr << "Print not implented" << endl;
        }
    };

    class Root : public Node {
        Location loc_;
        unique_ptr<Exp> root_;
        vector<VarDec *> mainVariableTable_;

    public:
        Root(Location loc, unique_ptr<Exp> root) :
                loc_(move(loc)), root_(move(root)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        Location &getLoc() {
            return loc_;
        }

        bool traverse(CodeGenContext &context);

        void print(int depth) override;
    };

    class Dec : public Node {
    protected:
        Location loc_;
        Identifier name_;
        string concreteType_;

    public:
        Dec(Location loc, Identifier name, string concreteType) :
                loc_(move(loc)), name_(name), concreteType_(move(concreteType)) {}

        Location &getLoc() {
            return loc_;
        }

        void print(int depth) override {
            std::cerr << "Print not implented" << endl;
        }

        virtual bool computeHeaderTraverse(vector<VarDec *> &,
                                           CodeGenContext &) = 0;

        virtual llvm::Value *computeHeaderCodegen(CodeGenContext &) = 0;

        const string &getName() {
            return name_.getName();
        }

        const string &getConcreteType() {
            return concreteType_;
        }
    };

    class Type {
        Location loc_;

    protected:
        Identifier name_;

    public:
        Type(Location loc, Identifier name) :
                loc_(move(loc)), name_(name) {}

        void setName(const Identifier &name) {
            name_ = name;
        }

        Identifier &getName() {
            return name_;
        }

        virtual ~Type() = default;

        virtual llvm::Type *traverse(std::set<string> &parentName,
                                     CodeGenContext &context) = 0;

        Location &getLoc() {
            return loc_;
        }

        virtual void print(int depth) = 0;
    };

    class SimpleVar : public Var {
        Identifier name_;

    public:
        SimpleVar(Location loc, Identifier name) :
                Var(move(loc)), name_(name) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class FieldVar : public Var {
        unique_ptr<Var> var_;
        Identifier field_;
        llvm::Type *type_{nullptr};
        size_t idx_{0u};

    public:
        FieldVar(Location loc, unique_ptr<Var> var,
                 Identifier field)
                : Var(move(loc)), var_(move(var)),
                  field_(move(field)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class SubscriptVar : public Var {
        unique_ptr<Var> var_;
        unique_ptr<Exp> exp_;
        llvm::Type *type_{nullptr};

    public:
        SubscriptVar(Location loc, unique_ptr<Var> var, unique_ptr<Exp> exp)
                : Var(move(loc)), var_(move(var)), exp_(move(exp)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class VarExp : public Exp {
        unique_ptr<Var> var_;

    public:
        VarExp(Location loc, unique_ptr<Var> var) :
                Exp(move(loc)), var_(move(var)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class NilExp : public Exp {
        llvm::Type *type_{nullptr};
    public:
        NilExp(Location loc) :
                Exp(move(loc)) {};

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void setType(llvm::Type *type) { type_ = type; }

        void print(int depth) override;
    };

    class IntExp : public Exp {
        int val_;

    public:
        IntExp(Location loc, int const &val) :
                Exp(move(loc)), val_(val) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class StringExp : public Exp {
        string val_;

    public:
        StringExp(Location loc, string val) :
                Exp(move(loc)), val_(move(val)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class CallExp : public Exp {
        Identifier func_;
        vector<unique_ptr<Exp>> args_;

    public:
        CallExp(Location loc, Identifier func,
                vector<unique_ptr<Exp>> args)
                : Exp(move(loc)), func_(move(func)), args_(move(args)) {
            reverse(args_.begin(), args_.end());
        }

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

// TODO: UnaryExp

    class BinaryExp : public Exp {
    public:
        enum Operator : char {
            ADD = '+',
            SUB = '-',
            MUL = '*',
            DIV = '/',
            LTH = '<',
            GTH = '>',
            EQU = '=',
            NEQU = '!',
            LEQU = '[',
            GEQU = ']',

            AND_ = '&',
            OR_ = '|',
            //    AND = '&',
            //    OR = '|',
            XOR = '^',
        };

    private:
        Operator op_;  // TODO: use enum
        unique_ptr<Exp> left_;
        unique_ptr<Exp> right_;

    public:
        BinaryExp(Location loc, Operator const &op,
                  unique_ptr<Exp> left, unique_ptr<Exp> right)
                : Exp(move(loc)), op_(op),
                  left_(move(left)), right_(move(right)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class Field {
        friend class RecordType;

        Location loc_;
        Identifier name_;
        Identifier typeName_;
        llvm::Type *type_{nullptr};
        VarDec *varDec_{nullptr};

    public:
        Field(Location loc,
              Identifier name,
              Identifier type) :
                loc_(move(move(loc))), name_(name), typeName_(type) {}

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context);

        llvm::Type *getType() const { return type_; }

        string &getName() {
            return name_.getName();
        }

        VarDec *getVar() const { return varDec_; }

        Location &getLoc() {
            return loc_;
        };

        void print(int depth);
    };

    class FieldExp : public Exp {
        friend class RecordExp;

        Identifier name_;
        unique_ptr<Exp> exp_;
        llvm::Type *type_;

    public:
        FieldExp(Location loc,
                 Identifier name,
                 unique_ptr<Exp> exp)
                : Exp(move(loc)),
                  name_(name),
                  exp_(move(exp)) {}

        string &getName() {
            return name_.getName();
        }

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class NameType : public Type {
        Identifier type_;

    public:
        NameType(Location loc, Identifier type) :

                Type(move(loc), type),
                type_(type) {}

        llvm::Type *traverse(std::set<string> &parentName,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class RecordExp : public Exp {
        friend class RecordType;

        unique_ptr<NameType> typeName_;
        vector<unique_ptr<FieldExp>> fieldExps_;
        llvm::Type *type_{nullptr};

    public:
        RecordExp(Location loc, unique_ptr<NameType> type,
                  vector<unique_ptr<FieldExp>> fieldExps)
                : Exp(move(loc)), typeName_(move(type)),
                  fieldExps_(move(fieldExps)) {
            reverse(fieldExps_.begin(), fieldExps_.end());
        }

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;

        const std::string getTypeName() {
            return typeName_->getName().getName();
        }
    };

    class SequenceExp : public Exp {
        vector<unique_ptr<Exp>> exps_;

    public:
        SequenceExp(Location loc, vector<unique_ptr<Exp>> exps) :
                Exp(move(loc)), exps_(move(exps)) {
            reverse(exps_.begin(), exps_.end());
        }

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class AssignExp : public Exp {
        unique_ptr<Var> var_;
        unique_ptr<Exp> exp_;

    public:
        AssignExp(Location loc, unique_ptr<Var> var, unique_ptr<Exp> exp)
                : Exp(move(loc)), var_(move(var)), exp_(move(exp)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class IfExp : public Exp {
        unique_ptr<Exp> test_;
        unique_ptr<Exp> then_;
        unique_ptr<Exp> else_;

    public:
        IfExp(Location loc, unique_ptr<Exp> test,
              unique_ptr<Exp> then, unique_ptr<Exp> elsee)
                : Exp(move(loc)), test_(move(test)),
                  then_(move(then)), else_(move(elsee)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class WhileExp : public Exp {
        unique_ptr<Exp> test_;
        unique_ptr<Exp> body_;

    public:
        WhileExp(Location loc, unique_ptr<Exp> test, unique_ptr<Exp> body)
                : Exp(move(loc)), test_(move(test)), body_(move(body)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class DoWhileExp : public Exp {
        unique_ptr<Exp> body_;
        unique_ptr<Exp> test_;

    public:
        DoWhileExp(Location loc, unique_ptr<Exp> body, unique_ptr<Exp> test)
                : Exp(move(loc)), body_(move(body)), test_(move(test)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class ForExp : public Exp {
        Identifier var_;
        unique_ptr<Exp> low_;
        unique_ptr<Exp> high_;
        unique_ptr<Exp> body_;
        VarDec *varDec_{nullptr};

    public:
        ForExp(Location loc,
               Identifier var,
               unique_ptr<Exp> low,
               unique_ptr<Exp> high,
               unique_ptr<Exp> body)
                : Exp(move(loc)),
                  var_(move(var)),
                  low_(move(low)),
                  high_(move(high)),
                  body_(move(body)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class BreakExp : public Exp {
    public:
        BreakExp(Location loc) : Exp(move(loc)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class LetExp : public Exp {
        vector<unique_ptr<Dec>> decs_;
        unique_ptr<Exp> body_;

    public:
        LetExp(Location loc, vector<unique_ptr<Dec>> decs, unique_ptr<Exp> body)
                : Exp(move(loc)), decs_(move(decs)), body_(move(body)) {
            reverse(decs_.begin(), decs_.end());
        }

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class ArrayExp : public Exp {
        unique_ptr<NameType> typeName_;
        unique_ptr<Exp> size_;
        unique_ptr<Exp> init_;
        llvm::Type *type_{nullptr};

    public:
        ArrayExp(Location loc, unique_ptr<NameType> type, unique_ptr<Exp> size, unique_ptr<Exp> init)
                : Exp(move(loc)), typeName_(move(type)), size_(move(size)), init_(move(init)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;

        const std::string getTypeName() {
            return typeName_->getName().getName();
        }
    };

    class Prototype {
        Location loc_;
        Identifier name_;
        vector<unique_ptr<Field>> params_;
        Identifier result_;
        llvm::Type *resultType_{nullptr};
        llvm::Function *function_{nullptr};
        VarDec *staticLink_{nullptr};
        llvm::StructType *frame{nullptr};

    public:
        Prototype(Location loc, Identifier name,
                  vector<unique_ptr<Field>> params,
                  Identifier result)
                : loc_(move(loc)), name_(name),
                  params_(move(params)),
                  result_(result) {
            reverse(params_.begin(), params_.end());
        }

        llvm::Function *codegen(CodeGenContext &context);

        string &getName() {
            return name_.getName();
        }

        void rename(Identifier name) {
            name_ = move(name);
        }

        const vector<unique_ptr<Field>> &getParams() const { return params_; }

        llvm::Type *getResultType() const { return resultType_; }

        llvm::FunctionType *traverse(vector<VarDec *> &variableTable,
                                     CodeGenContext &context);

        llvm::StructType *getFrame() const { return frame; }

        llvm::Function *getFunction() const { return function_; }

        VarDec *getStaticLink() const { return staticLink_; }

        Location &getLoc() {
            return loc_;
        }

        Identifier &getResult() {
            return result_;
        }

        void print(int depth);
    };

    class FunctionDec : public Dec {
        unique_ptr<Prototype> proto_;
        unique_ptr<Exp> body_;
        vector<VarDec *> variableTable_;
        size_t level_{0u};

    public:
        FunctionDec(Location loc, Identifier name,
                    unique_ptr<Prototype> proto, unique_ptr<Exp> body)
                : Dec(move(loc), move(name), "FunctionDec"), proto_(move(proto)), body_(move(body)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        Prototype &getProto() const {
            return *proto_;
        }

        size_t getLevel() const {
            return level_;
        }

        void print(int depth) override;

        bool computeHeaderTraverse(vector<VarDec *> &vector,
                                   CodeGenContext &context) override;

        llvm::Value *computeHeaderCodegen(CodeGenContext &context) override;
    };

    class NameType;

    class VarDec : public Dec {
        unique_ptr<NameType> typeName_;
        unique_ptr<Exp> init_{nullptr};
        size_t offset_;
        size_t level_;
        llvm::Type *type_{nullptr};
        bool global{false};

    public:
        VarDec(Location loc, Identifier name, unique_ptr<NameType> type, unique_ptr<Exp> init)
                : Dec(move(loc), move(name), "VarDec"), typeName_(move(type)), init_(move(init)) {}

        VarDec(Location loc, Identifier name, llvm::Type *type, size_t const &offset,
               size_t const &level)
                : Dec(move(loc), move(name), "VarDec"), offset_(offset), level_(level), type_(type) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *getType() const { return type_; }

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;

        bool computeHeaderTraverse(vector<VarDec *> &vector,
                                   CodeGenContext &context) override;

        llvm::Value *computeHeaderCodegen(CodeGenContext &context) override;

        bool isGlobal() {
            return global;
        }
    };

    class TypeDec : public Dec {
        unique_ptr<Type> type_;

    public:
        TypeDec(Location loc,
                Identifier name,
                unique_ptr<Type> type)
                : Dec(move(loc), move(name), "TypeDec"), type_(move(type)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void print(int depth) override;

        bool computeHeaderTraverse(vector<VarDec *> &vector,
                                   CodeGenContext &context) override;

        llvm::Value *computeHeaderCodegen(CodeGenContext &context) override;
    };

    class RecordType : public Type {
        friend class RecordExp;

        friend class FieldVar;

        vector<unique_ptr<Field>> fields_;

    public:
        RecordType(Location loc, vector<unique_ptr<Field>> fields) :
                Type(move(loc), Identifier(loc, "")), fields_(move(fields)) {
            reverse(fields_.begin(), fields_.end());
        }

        llvm::Type *traverse(std::set<string> &parentName,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

    class ArrayType : public Type {
        Identifier type_;

    protected:
    public:
        ArrayType(Location loc, Identifier type) :
                Type(move(loc), Identifier(loc, "")), type_(type) {}

        llvm::Type *traverse(std::set<string> &parentName,
                             CodeGenContext &context) override;

        void print(int depth) override;
    };

}  // namespace AST

#endif  // AST_HPP

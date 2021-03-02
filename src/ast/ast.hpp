#ifndef AST_H
#define AST_H

#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include "utils/codegencontext.hpp"
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

class CodeGenContext;
namespace AST {
    using llvm::Value;
    using std::move;
    using std::reverse;
    using std::set;
    using std::string;
    using std::unique_ptr;
    using std::vector;

    class VarDec;

    class Node {
        size_t pos_;

    public:
        virtual ~Node() = default;

        virtual Value *codegen(CodeGenContext &context) = 0;

        void setPos(const size_t &pos) { pos_ = pos; }

        virtual llvm::Type *traverse(vector<VarDec *> &, CodeGenContext &) = 0;
    };

    class Location {
    public:
        int first_line_;
        int first_column_;

    public:
        Location(int first_line, int first_column) :
                first_line_(first_line), first_column_(first_column) {}
    };

    class Var : public Node {
        Location loc_;

    public:
        Var(Location loc) : loc_(loc) {}

        Location &getLocation();
    };

    class Exp : public Node {
        Location loc_;

    public:
        Exp(Location loc) : loc_(loc) {}

        Location &getLocation();
    };

    class Root : public Node {
        Location loc_;
        unique_ptr<Exp> root_;
        vector<VarDec *> mainVariableTable_;

    public:
        Root(Location loc, unique_ptr<Exp> root) :
                loc_(loc), root_(move(root)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        Location &getLocation();

        bool semanticAnalisys();
    };

    class Dec : public Node {
        Location loc_;

    protected:
        string name_;

    public:
        Dec(Location loc, string name) :
                loc_(loc), name_(move(name)) {}

        Location &getLocation();
    };

    class Type {
        Location loc_;

    protected:
        string name_;

    public:
        Type(Location loc) : loc_(loc) {}

        void setName(string name) { name_ = move(name); }

        const string &getName() const { return name_; }

        virtual ~Type() = default;

        virtual llvm::Type *traverse(std::set<string> &parentName,
                                     CodeGenContext &context) = 0;

        Location &getLocation();
    };

    class SimpleVar : public Var {
        string name_;

    public:
        SimpleVar(Location loc, string name) :
                Var(loc), name_(move(name)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class FieldVar : public Var {
        unique_ptr<Var> var_;
        string field_;
        llvm::Type *type_{nullptr};
        size_t idx_{0u};

    public:
        FieldVar(Location loc, unique_ptr<Var> var, string field)
                : Var(loc), var_(move(var)), field_(move(field)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class SubscriptVar : public Var {
        unique_ptr<Var> var_;
        unique_ptr<Exp> exp_;
        llvm::Type *type_{nullptr};

    public:
        SubscriptVar(Location loc, unique_ptr<Var> var, unique_ptr<Exp> exp)
                : Var(loc), var_(move(var)), exp_(move(exp)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class VarExp : public Exp {
        unique_ptr<Var> var_;

    public:
        VarExp(Location loc, unique_ptr<Var> var) :
                Exp(loc), var_(move(var)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class NilExp : public Exp {
        // dummy body
        llvm::Type *type_{nullptr};
    public:
        NilExp(Location loc) :
                Exp(loc) {};

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        void setType(llvm::Type *type) { type_ = type; }
    };

    class IntExp : public Exp {
        int val_;

    public:
        IntExp(Location loc, int const &val) :
                Exp(loc), val_(val) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class StringExp : public Exp {
        string val_;

    public:
        StringExp(Location loc, string val) :
                Exp(loc), val_(move(val)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class CallExp : public Exp {
        string func_;
        vector<unique_ptr<Exp>> args_;

    public:
        CallExp(Location loc, string func, vector<unique_ptr<Exp>> args)
                : Exp(loc), func_(move(func)), args_(move(args)) {
            reverse(args_.begin(), args_.end());
        }

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
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
            LEQ = '[',
            GEQ = ']',

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
                : Exp(loc), op_(op),
                  left_(move(left)), right_(move(right)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class Field {
        friend class RecordType;

        Location loc_;
        string name_;
        string typeName_;
        llvm::Type *type_{nullptr};
        VarDec *varDec_{nullptr};

    public:
        Field(Location loc, string name, string type) :
                loc_(loc), name_(move(name)), typeName_(move(type)) {}

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context);

        llvm::Type *getType() const { return type_; }

        const string &getName() const { return name_; }

        VarDec *getVar() const { return varDec_; }

        Location &getLocation();
    };

    class FieldExp : public Exp {
        friend class RecordExp;

        string name_;
        unique_ptr<Exp> exp_;
        llvm::Type *type_;

    public:
        FieldExp(Location loc, string name, unique_ptr<Exp> exp)
                : Exp(loc), name_(move(name)), exp_(move(exp)) {}

        const string &getName() const { return name_; }

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class RecordExp : public Exp {
        friend class RecordType;

        int first_line_;
        int first_column_;
        string typeName_;
        vector<unique_ptr<FieldExp>> fieldExps_;
        llvm::Type *type_{nullptr};

    public:
        RecordExp(Location loc, string type, vector<unique_ptr<FieldExp>> fieldExps)
                : Exp(loc), typeName_(move(type)), fieldExps_(move(fieldExps)) {
            reverse(fieldExps_.begin(), fieldExps_.end());
        }

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class SequenceExp : public Exp {
        vector<unique_ptr<Exp>> exps_;

    public:
        SequenceExp(Location loc, vector<unique_ptr<Exp>> exps) :
                Exp(loc), exps_(move(exps)) {
            reverse(exps_.begin(), exps_.end());
        }

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class AssignExp : public Exp {
        unique_ptr<Var> var_;
        unique_ptr<Exp> exp_;

    public:
        AssignExp(Location loc, unique_ptr<Var> var, unique_ptr<Exp> exp)
                : Exp(loc), var_(move(var)), exp_(move(exp)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class IfExp : public Exp {
        unique_ptr<Exp> test_;
        unique_ptr<Exp> then_;
        unique_ptr<Exp> else_;

    public:
        IfExp(Location loc, unique_ptr<Exp> test, unique_ptr<Exp> then, unique_ptr<Exp> elsee)
                : Exp(loc), test_(move(test)), then_(move(then)), else_(move(elsee)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class WhileExp : public Exp {
        unique_ptr<Exp> test_;
        unique_ptr<Exp> body_;

    public:
        WhileExp(Location loc, unique_ptr<Exp> test, unique_ptr<Exp> body)
                : Exp(loc), test_(move(test)), body_(move(body)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class DoWhileExp : public Exp {
        unique_ptr<Exp> body_;
        unique_ptr<Exp> test_;

    public:
        DoWhileExp(Location loc, unique_ptr<Exp> body, unique_ptr<Exp> test)
                : Exp(loc), body_(move(body)), test_(move(test)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class ForExp : public Exp {
        string var_;
        unique_ptr<Exp> low_;
        unique_ptr<Exp> high_;
        unique_ptr<Exp> body_;
        // bool escape;
        VarDec *varDec_{nullptr};

    public:
        ForExp(Location loc, string var, unique_ptr<Exp> low, unique_ptr<Exp> high,
               unique_ptr<Exp> body)
                : Exp(loc),
                  var_(move(var)),
                  low_(move(low)),
                  high_(move(high)),
                  body_(move(body)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class BreakExp : public Exp {
        // dummpy body
    public:
        BreakExp(Location loc) : Exp(loc) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

    };

    class LetExp : public Exp {
        vector<unique_ptr<Dec>> decs_;
        unique_ptr<Exp> body_;

    public:
        LetExp(Location loc, vector<unique_ptr<Dec>> decs, unique_ptr<Exp> body)
                : Exp(loc), decs_(move(decs)), body_(move(body)) {
            reverse(decs_.begin(), decs_.end());
        }

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class ArrayExp : public Exp {
        string typeName_;
        unique_ptr<Exp> size_;
        unique_ptr<Exp> init_;
        llvm::Type *type_{nullptr};

    public:
        ArrayExp(Location loc, string type, unique_ptr<Exp> size, unique_ptr<Exp> init)
                : Exp(loc), typeName_(move(type)), size_(move(size)), init_(move(init)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class Prototype {
        Location loc_;
        string name_;
        vector<unique_ptr<Field>> params_;
        string result_;
        llvm::Type *resultType_{nullptr};
        llvm::Function *function_{nullptr};
        VarDec *staticLink_{nullptr};
        llvm::StructType *frame{nullptr};

    public:
        Prototype(Location loc, string name,
                  vector<unique_ptr<Field>> params, string result)
                : loc_(loc), name_(move(name)), params_(move(params)), result_(move(result)) {
            reverse(params_.begin(), params_.end());
        }

        llvm::Function *codegen(CodeGenContext &context);

        const string &getName() const { return name_; }

        void rename(string name) { name_ = move(name); }

        const vector<unique_ptr<Field>> &getParams() const { return params_; }

        llvm::Type *getResultType() const { return resultType_; }

        llvm::FunctionType *traverse(vector<VarDec *> &variableTable,
                                     CodeGenContext &context);

        llvm::StructType *getFrame() const { return frame; }

        llvm::Function *getFunction() const { return function_; }

        VarDec *getStaticLink() const { return staticLink_; }

        Location &getLocation();
    };

    class FunctionDec : public Dec {
        unique_ptr<Prototype> proto_;
        unique_ptr<Exp> body_;
        vector<VarDec *> variableTable_;
        size_t level_{0u};

    public:
        FunctionDec(Location loc, string name,
                    unique_ptr<Prototype> proto, unique_ptr<Exp> body)
                : Dec(loc, move(name)), proto_(move(proto)), body_(move(body)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;

        Prototype &getProto() const { return *proto_; }

        size_t getLevel() const { return level_; }
    };

    class VarDec : public Dec {
        string typeName_;
        unique_ptr<Exp> init_{nullptr};
        // bool escape;
        size_t offset_;
        size_t level_;
        llvm::Type *type_{nullptr};

    public:
        VarDec(Location loc, string name, string type, unique_ptr<Exp> init)
                : Dec(loc, move(name)), typeName_(move(type)), init_(move(init)) {}

        VarDec(Location loc, string name, llvm::Type *type, size_t const &offset,
               size_t const &level)
                : Dec(loc, move(name)), offset_(offset), level_(level), type_(type) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *getType() const { return type_; }

        const string &getName() const { return name_; }

        llvm::Value *read(CodeGenContext &context) const;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class TypeDec : public Dec {
        unique_ptr<Type> type_;

    public:
        TypeDec(Location loc, string name, unique_ptr<Type> type)
                : Dec(loc, move(name)), type_(move(type)) {}

        Value *codegen(CodeGenContext &context) override;

        llvm::Type *traverse(vector<VarDec *> &variableTable,
                             CodeGenContext &context) override;
    };

    class NameType : public Type {
        string type_;

    public:
        NameType(Location loc, string type) :
                Type(loc), type_(move(type)) {}

        llvm::Type *traverse(std::set<string> &parentName,
                             CodeGenContext &context) override;
    };

    class RecordType : public Type {
        friend class RecordExp;

        friend class FieldVar;

        vector<unique_ptr<Field>> fields_;

    public:
        RecordType(Location loc, vector<unique_ptr<Field>> fields) :
                Type(loc), fields_(move(fields)) {
            reverse(fields_.begin(), fields_.end());
        }

        llvm::Type *traverse(std::set<string> &parentName,
                             CodeGenContext &context) override;
    };

    class ArrayType : public Type {
        string type_;

    protected:
    public:
        ArrayType(Location loc, string type) :
                Type(loc), type_(move(type)) {}

        llvm::Type *traverse(std::set<string> &parentName,
                             CodeGenContext &context) override;
    };

}  // namespace AST

#endif  // AST_H

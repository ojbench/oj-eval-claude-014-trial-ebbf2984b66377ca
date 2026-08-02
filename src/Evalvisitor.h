#pragma once
#ifndef PYTHON_INTERPRETER_EVALVISITOR_H
#define PYTHON_INTERPRETER_EVALVISITOR_H

#include "Python3ParserBaseVisitor.h"
#include "BigInt.h"
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <iomanip>
#include <stdexcept>

// Value types
enum class ValueType {
    NONE,
    BOOL,
    INT,
    FLOAT,
    STRING,
    TUPLE,
    FUNCTION
};

struct Value {
    ValueType type;
    bool boolVal;
    BigInt intVal;
    double floatVal;
    std::string strVal;
    std::vector<Value> tupleVal;

    // For functions
    std::string funcName;
    std::vector<std::string> params;
    std::vector<std::pair<std::string, Value>> defaultParams;
    Python3Parser::SuiteContext* funcBody;

    Value() : type(ValueType::NONE), boolVal(false), floatVal(0.0), funcBody(nullptr) {}

    bool toBool() const {
        switch (type) {
            case ValueType::NONE: return false;
            case ValueType::BOOL: return boolVal;
            case ValueType::INT: return !intVal.isZero();
            case ValueType::FLOAT: return floatVal != 0.0;
            case ValueType::STRING: return !strVal.empty();
            default: return false;
        }
    }

    std::string toString() const {
        switch (type) {
            case ValueType::NONE: return "None";
            case ValueType::BOOL: return boolVal ? "True" : "False";
            case ValueType::INT: return intVal.toString();
            case ValueType::FLOAT: {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(6) << floatVal;
                return oss.str();
            }
            case ValueType::STRING: return strVal;
            case ValueType::TUPLE: {
                std::string result = "(";
                for (size_t i = 0; i < tupleVal.size(); ++i) {
                    if (i > 0) result += ", ";
                    result += tupleVal[i].toString();
                }
                if (tupleVal.size() == 1) result += ",";
                result += ")";
                return result;
            }
            default: return "";
        }
    }
};

// Exception classes for control flow
class BreakException : public std::exception {};
class ContinueException : public std::exception {};
class ReturnException : public std::exception {
public:
    Value value;
    ReturnException(const Value& v) : value(v) {}
};

class EvalVisitor : public Python3ParserBaseVisitor {
private:
    std::vector<std::map<std::string, Value>> scopes;
    std::map<std::string, Value> globalScope;

    void enterScope() {
        scopes.push_back(std::map<std::string, Value>());
    }

    void exitScope() {
        if (!scopes.empty()) {
            scopes.pop_back();
        }
    }

    void setVariable(const std::string& name, const Value& value) {
        if (scopes.empty()) {
            globalScope[name] = value;
        } else {
            scopes.back()[name] = value;
        }
    }

    Value getVariable(const std::string& name) {
        // Search from innermost to outermost scope
        for (int i = scopes.size() - 1; i >= 0; --i) {
            if (scopes[i].find(name) != scopes[i].end()) {
                return scopes[i][name];
            }
        }
        // Check global scope
        if (globalScope.find(name) != globalScope.end()) {
            return globalScope[name];
        }
        // Variable not found - return None
        return Value();
    }

    Value convertToInt(const Value& val) {
        Value result;
        result.type = ValueType::INT;
        switch (val.type) {
            case ValueType::BOOL:
                result.intVal = BigInt(val.boolVal ? 1 : 0);
                break;
            case ValueType::INT:
                result.intVal = val.intVal;
                break;
            case ValueType::FLOAT:
                result.intVal = BigInt((long long)val.floatVal);
                break;
            case ValueType::STRING:
                result.intVal = BigInt(val.strVal);
                break;
            default:
                result.intVal = BigInt(0);
        }
        return result;
    }

    Value convertToFloat(const Value& val) {
        Value result;
        result.type = ValueType::FLOAT;
        switch (val.type) {
            case ValueType::BOOL:
                result.floatVal = val.boolVal ? 1.0 : 0.0;
                break;
            case ValueType::INT:
                result.floatVal = val.intVal.toDouble();
                break;
            case ValueType::FLOAT:
                result.floatVal = val.floatVal;
                break;
            case ValueType::STRING:
                result.floatVal = std::stod(val.strVal);
                break;
            default:
                result.floatVal = 0.0;
        }
        return result;
    }

    Value convertToString(const Value& val) {
        Value result;
        result.type = ValueType::STRING;
        result.strVal = val.toString();
        return result;
    }

    Value convertToBool(const Value& val) {
        Value result;
        result.type = ValueType::BOOL;
        result.boolVal = val.toBool();
        return result;
    }

public:
    EvalVisitor() {}

    std::any visitFile_input(Python3Parser::File_inputContext *ctx) override;
    std::any visitFuncdef(Python3Parser::FuncdefContext *ctx) override;
    std::any visitParameters(Python3Parser::ParametersContext *ctx) override;
    std::any visitTypedargslist(Python3Parser::TypedargslistContext *ctx) override;
    std::any visitStmt(Python3Parser::StmtContext *ctx) override;
    std::any visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) override;
    std::any visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) override;
    std::any visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) override;
    std::any visitAugassign(Python3Parser::AugassignContext *ctx) override;
    std::any visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) override;
    std::any visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) override;
    std::any visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) override;
    std::any visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) override;
    std::any visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) override;
    std::any visitIf_stmt(Python3Parser::If_stmtContext *ctx) override;
    std::any visitWhile_stmt(Python3Parser::While_stmtContext *ctx) override;
    std::any visitSuite(Python3Parser::SuiteContext *ctx) override;
    std::any visitTest(Python3Parser::TestContext *ctx) override;
    std::any visitOr_test(Python3Parser::Or_testContext *ctx) override;
    std::any visitAnd_test(Python3Parser::And_testContext *ctx) override;
    std::any visitNot_test(Python3Parser::Not_testContext *ctx) override;
    std::any visitComparison(Python3Parser::ComparisonContext *ctx) override;
    std::any visitComp_op(Python3Parser::Comp_opContext *ctx) override;
    std::any visitArith_expr(Python3Parser::Arith_exprContext *ctx) override;
    std::any visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx) override;
    std::any visitTerm(Python3Parser::TermContext *ctx) override;
    std::any visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx) override;
    std::any visitFactor(Python3Parser::FactorContext *ctx) override;
    std::any visitAtom_expr(Python3Parser::Atom_exprContext *ctx) override;
    std::any visitTrailer(Python3Parser::TrailerContext *ctx) override;
    std::any visitAtom(Python3Parser::AtomContext *ctx) override;
    std::any visitFormat_string(Python3Parser::Format_stringContext *ctx) override;
    std::any visitTestlist(Python3Parser::TestlistContext *ctx) override;
    std::any visitArglist(Python3Parser::ArglistContext *ctx) override;
    std::any visitArgument(Python3Parser::ArgumentContext *ctx) override;

    // Helper methods for operations
    Value performAddition(const Value& left, const Value& right);
    Value performSubtraction(const Value& left, const Value& right);
    Value performMultiplication(const Value& left, const Value& right);
    Value performDivision(const Value& left, const Value& right);
    Value performFloorDivision(const Value& left, const Value& right);
    Value performModulo(const Value& left, const Value& right);
    Value performComparison(const Value& left, const Value& right, const std::string& op);
    Value callBuiltinFunction(const std::string& name, const std::vector<Value>& args);
    Value callUserFunction(const Value& func, const std::vector<Value>& args,
                           const std::map<std::string, Value>& kwargs);
    std::string evaluateFormatString(const std::string& fmtStr);
};

#endif//PYTHON_INTERPRETER_EVALVISITOR_H

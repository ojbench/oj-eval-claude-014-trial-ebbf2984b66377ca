#include "Evalvisitor.h"
#include "Python3Lexer.h"
#include "Python3Parser.h"
#include "antlr4-runtime.h"
#include <cmath>

using namespace antlr4;
using namespace antlr4::tree;

std::any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitFuncdef(Python3Parser::FuncdefContext *ctx) {
    Value func;
    func.type = ValueType::FUNCTION;
    func.funcName = ctx->NAME()->getText();
    func.funcBody = ctx->suite();

    // Parse parameters
    if (ctx->parameters() && ctx->parameters()->typedargslist()) {
        auto paramList = ctx->parameters()->typedargslist();
        auto tfpdefs = paramList->tfpdef();
        auto tests = paramList->test();

        int numDefaults = tests.size();
        int numParams = tfpdefs.size();
        int numNonDefaults = numParams - numDefaults;

        for (int i = 0; i < numNonDefaults; ++i) {
            func.params.push_back(tfpdefs[i]->NAME()->getText());
        }

        for (int i = 0; i < numDefaults; ++i) {
            std::string paramName = tfpdefs[numNonDefaults + i]->NAME()->getText();
            Value defaultVal = std::any_cast<Value>(visit(tests[i]));
            func.defaultParams.push_back({paramName, defaultVal});
        }
    }

    setVariable(func.funcName, func);
    return Value();
}

std::any EvalVisitor::visitStmt(Python3Parser::StmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) {
    auto testlists = ctx->testlist();

    if (testlists.size() == 1) {
        // Just an expression, evaluate it
        return visit(testlists[0]);
    }

    // Assignment or chained assignment
    Value rightValue = std::any_cast<Value>(visit(testlists.back()));

    for (int i = testlists.size() - 2; i >= 0; --i) {
        auto testlist = testlists[i];
        auto tests = testlist->test();

        if (tests.size() == 1) {
            // Single assignment
            auto test = tests[0];
            if (test->or_test() && test->or_test()->and_test().size() == 1) {
                auto andTest = test->or_test()->and_test(0);
                if (andTest->not_test().size() == 1) {
                    auto notTest = andTest->not_test(0);
                    if (notTest->comparison()) {
                        auto comp = notTest->comparison();
                        if (comp->arith_expr().size() == 1) {
                            auto arith = comp->arith_expr(0);
                            if (arith->term().size() == 1) {
                                auto term = arith->term(0);
                                if (term->factor().size() == 1) {
                                    auto factor = term->factor(0);
                                    if (factor->atom_expr()) {
                                        auto atomExpr = factor->atom_expr();
                                        if (atomExpr->atom() && atomExpr->atom()->NAME()) {
                                            std::string varName = atomExpr->atom()->NAME()->getText();
                                            setVariable(varName, rightValue);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // Multiple assignment: a, b = 1, 2
            if (rightValue.type == ValueType::TUPLE) {
                for (size_t j = 0; j < tests.size() && j < rightValue.tupleVal.size(); ++j) {
                    auto test = tests[j];
                    // Extract variable name (simplified)
                    if (test->or_test() && test->or_test()->and_test().size() == 1) {
                        auto andTest = test->or_test()->and_test(0);
                        if (andTest->not_test().size() == 1) {
                            auto notTest = andTest->not_test(0);
                            if (notTest->comparison()) {
                                auto comp = notTest->comparison();
                                if (comp->arith_expr().size() == 1) {
                                    auto arith = comp->arith_expr(0);
                                    if (arith->term().size() == 1) {
                                        auto term = arith->term(0);
                                        if (term->factor().size() == 1) {
                                            auto factor = term->factor(0);
                                            if (factor->atom_expr()) {
                                                auto atomExpr = factor->atom_expr();
                                                if (atomExpr->atom() && atomExpr->atom()->NAME()) {
                                                    std::string varName = atomExpr->atom()->NAME()->getText();
                                                    setVariable(varName, rightValue.tupleVal[j]);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Check for augmented assignment
    if (ctx->augassign()) {
        std::string op = ctx->augassign()->getText();
        std::string varName;

        auto testlist = testlists[0];
        auto test = testlist->test(0);
        if (test->or_test() && test->or_test()->and_test().size() == 1) {
            auto andTest = test->or_test()->and_test(0);
            if (andTest->not_test().size() == 1) {
                auto notTest = andTest->not_test(0);
                if (notTest->comparison()) {
                    auto comp = notTest->comparison();
                    if (comp->arith_expr().size() == 1) {
                        auto arith = comp->arith_expr(0);
                        if (arith->term().size() == 1) {
                            auto term = arith->term(0);
                            if (term->factor().size() == 1) {
                                auto factor = term->factor(0);
                                if (factor->atom_expr()) {
                                    auto atomExpr = factor->atom_expr();
                                    if (atomExpr->atom() && atomExpr->atom()->NAME()) {
                                        varName = atomExpr->atom()->NAME()->getText();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Value leftVal = getVariable(varName);
        Value rightVal = std::any_cast<Value>(visit(testlists[1]));
        Value result;

        if (op == "+=") {
            result = performAddition(leftVal, rightVal);
        } else if (op == "-=") {
            result = performSubtraction(leftVal, rightVal);
        } else if (op == "*=") {
            result = performMultiplication(leftVal, rightVal);
        } else if (op == "/=") {
            result = performDivision(leftVal, rightVal);
        } else if (op == "//=") {
            result = performFloorDivision(leftVal, rightVal);
        } else if (op == "%=") {
            result = performModulo(leftVal, rightVal);
        }

        setVariable(varName, result);
    }

    return rightValue;
}

std::any EvalVisitor::visitAugassign(Python3Parser::AugassignContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) {
    throw BreakException();
}

std::any EvalVisitor::visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) {
    throw ContinueException();
}

std::any EvalVisitor::visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) {
    if (ctx->testlist()) {
        Value result = std::any_cast<Value>(visit(ctx->testlist()));
        throw ReturnException(result);
    }
    throw ReturnException(Value());
}

std::any EvalVisitor::visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitIf_stmt(Python3Parser::If_stmtContext *ctx) {
    auto tests = ctx->test();
    auto suites = ctx->suite();

    for (size_t i = 0; i < tests.size(); ++i) {
        Value condition = std::any_cast<Value>(visit(tests[i]));
        if (condition.toBool()) {
            visit(suites[i]);
            return Value();
        }
    }

    // else clause
    if (suites.size() > tests.size()) {
        visit(suites.back());
    }

    return Value();
}

std::any EvalVisitor::visitWhile_stmt(Python3Parser::While_stmtContext *ctx) {
    while (true) {
        Value condition = std::any_cast<Value>(visit(ctx->test()));
        if (!condition.toBool()) break;

        try {
            visit(ctx->suite());
        } catch (BreakException&) {
            break;
        } catch (ContinueException&) {
            continue;
        }
    }
    return Value();
}

std::any EvalVisitor::visitSuite(Python3Parser::SuiteContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitTest(Python3Parser::TestContext *ctx) {
    return visit(ctx->or_test());
}

std::any EvalVisitor::visitOr_test(Python3Parser::Or_testContext *ctx) {
    auto andTests = ctx->and_test();
    Value result = std::any_cast<Value>(visit(andTests[0]));

    for (size_t i = 1; i < andTests.size(); ++i) {
        if (result.toBool()) {
            return result;
        }
        result = std::any_cast<Value>(visit(andTests[i]));
    }
    return result;
}

std::any EvalVisitor::visitAnd_test(Python3Parser::And_testContext *ctx) {
    auto notTests = ctx->not_test();
    Value result = std::any_cast<Value>(visit(notTests[0]));

    for (size_t i = 1; i < notTests.size(); ++i) {
        if (!result.toBool()) {
            return result;
        }
        result = std::any_cast<Value>(visit(notTests[i]));
    }
    return result;
}

std::any EvalVisitor::visitNot_test(Python3Parser::Not_testContext *ctx) {
    if (ctx->NOT()) {
        Value val = std::any_cast<Value>(visit(ctx->not_test()));
        Value result;
        result.type = ValueType::BOOL;
        result.boolVal = !val.toBool();
        return result;
    }
    return visit(ctx->comparison());
}

std::any EvalVisitor::visitComparison(Python3Parser::ComparisonContext *ctx) {
    auto arithExprs = ctx->arith_expr();
    auto compOps = ctx->comp_op();

    if (compOps.empty()) {
        return visit(arithExprs[0]);
    }

    // Chained comparison: evaluate each expression once
    std::vector<Value> values;
    for (auto arith : arithExprs) {
        values.push_back(std::any_cast<Value>(visit(arith)));
    }

    Value result;
    result.type = ValueType::BOOL;
    result.boolVal = true;

    for (size_t i = 0; i < compOps.size(); ++i) {
        std::string op = compOps[i]->getText();
        Value cmpResult = performComparison(values[i], values[i+1], op);
        if (!cmpResult.toBool()) {
            result.boolVal = false;
            break;
        }
    }

    return result;
}

std::any EvalVisitor::visitComp_op(Python3Parser::Comp_opContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitArith_expr(Python3Parser::Arith_exprContext *ctx) {
    auto terms = ctx->term();
    Value result = std::any_cast<Value>(visit(terms[0]));

    for (size_t i = 1; i < terms.size(); ++i) {
        std::string op = ctx->addorsub_op(i-1)->getText();
        Value right = std::any_cast<Value>(visit(terms[i]));
        if (op == "+") {
            result = performAddition(result, right);
        } else {
            result = performSubtraction(result, right);
        }
    }
    return result;
}

std::any EvalVisitor::visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitTerm(Python3Parser::TermContext *ctx) {
    auto factors = ctx->factor();
    Value result = std::any_cast<Value>(visit(factors[0]));

    for (size_t i = 1; i < factors.size(); ++i) {
        std::string op = ctx->muldivmod_op(i-1)->getText();
        Value right = std::any_cast<Value>(visit(factors[i]));
        if (op == "*") {
            result = performMultiplication(result, right);
        } else if (op == "/") {
            result = performDivision(result, right);
        } else if (op == "//") {
            result = performFloorDivision(result, right);
        } else if (op == "%") {
            result = performModulo(result, right);
        }
    }
    return result;
}

std::any EvalVisitor::visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitFactor(Python3Parser::FactorContext *ctx) {
    if (ctx->ADD()) {
        return visit(ctx->factor());
    } else if (ctx->MINUS()) {
        Value val = std::any_cast<Value>(visit(ctx->factor()));
        Value result;
        if (val.type == ValueType::INT) {
            result.type = ValueType::INT;
            result.intVal = -val.intVal;
        } else if (val.type == ValueType::FLOAT) {
            result.type = ValueType::FLOAT;
            result.floatVal = -val.floatVal;
        } else {
            result = val;
        }
        return result;
    }
    return visit(ctx->atom_expr());
}

std::any EvalVisitor::visitAtom_expr(Python3Parser::Atom_exprContext *ctx) {
    Value result = std::any_cast<Value>(visit(ctx->atom()));

    // Handle function call
    if (ctx->trailer()) {
        auto trailer = ctx->trailer();
        if (trailer->OPEN_PAREN()) {
            // Function call
            std::string funcName;
            if (ctx->atom()->NAME()) {
                funcName = ctx->atom()->NAME()->getText();
            }

            std::vector<Value> args;
            std::map<std::string, Value> kwargs;

            if (trailer->arglist()) {
                auto arglist = trailer->arglist();
                auto arguments = arglist->argument();

                for (auto arg : arguments) {
                    if (arg->test().size() == 2) {
                        // Keyword argument
                        std::string key = arg->test(0)->getText();
                        Value val = std::any_cast<Value>(visit(arg->test(1)));
                        kwargs[key] = val;
                    } else {
                        // Positional argument
                        Value val = std::any_cast<Value>(visit(arg->test(0)));
                        args.push_back(val);
                    }
                }
            }

            // Check if it's a built-in function
            if (funcName == "print" || funcName == "int" || funcName == "float" ||
                funcName == "str" || funcName == "bool") {
                result = callBuiltinFunction(funcName, args);
            } else {
                Value func = getVariable(funcName);
                if (func.type == ValueType::FUNCTION) {
                    result = callUserFunction(func, args, kwargs);
                }
            }
        }
    }

    return result;
}

std::any EvalVisitor::visitTrailer(Python3Parser::TrailerContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitAtom(Python3Parser::AtomContext *ctx) {
    Value result;

    if (ctx->NAME()) {
        std::string name = ctx->NAME()->getText();
        result = getVariable(name);
    } else if (ctx->NUMBER()) {
        std::string numStr = ctx->NUMBER()->getText();
        if (numStr.find('.') != std::string::npos) {
            result.type = ValueType::FLOAT;
            result.floatVal = std::stod(numStr);
        } else {
            result.type = ValueType::INT;
            result.intVal = BigInt(numStr);
        }
    } else if (ctx->TRUE()) {
        result.type = ValueType::BOOL;
        result.boolVal = true;
    } else if (ctx->FALSE()) {
        result.type = ValueType::BOOL;
        result.boolVal = false;
    } else if (ctx->NONE()) {
        result.type = ValueType::NONE;
    } else if (ctx->STRING().size() > 0) {
        result.type = ValueType::STRING;
        result.strVal = "";
        for (auto str : ctx->STRING()) {
            std::string s = str->getText();
            // Remove quotes
            if (s.length() >= 2) {
                s = s.substr(1, s.length() - 2);
            }
            result.strVal += s;
        }
    } else if (ctx->format_string()) {
        result = std::any_cast<Value>(visit(ctx->format_string()));
    } else if (ctx->test()) {
        result = std::any_cast<Value>(visit(ctx->test()));
    }

    return result;
}

std::any EvalVisitor::visitFormat_string(Python3Parser::Format_stringContext *ctx) {
    std::string text = ctx->getText();
    // Remove f" at the beginning and " at the end
    if (text.length() >= 3 && text[0] == 'f') {
        text = text.substr(2, text.length() - 3);
    }

    Value result;
    result.type = ValueType::STRING;
    result.strVal = evaluateFormatString(text);
    return result;
}

std::any EvalVisitor::visitTestlist(Python3Parser::TestlistContext *ctx) {
    auto tests = ctx->test();
    if (tests.size() == 1) {
        return visit(tests[0]);
    }

    Value result;
    result.type = ValueType::TUPLE;
    for (auto test : tests) {
        result.tupleVal.push_back(std::any_cast<Value>(visit(test)));
    }
    return result;
}

std::any EvalVisitor::visitArglist(Python3Parser::ArglistContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitArgument(Python3Parser::ArgumentContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitParameters(Python3Parser::ParametersContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitTypedargslist(Python3Parser::TypedargslistContext *ctx) {
    return visitChildren(ctx);
}

// Helper methods implementations

Value EvalVisitor::performAddition(const Value& left, const Value& right) {
    Value result;

    // String concatenation
    if (left.type == ValueType::STRING && right.type == ValueType::STRING) {
        result.type = ValueType::STRING;
        result.strVal = left.strVal + right.strVal;
        return result;
    }

    // Integer + Integer
    if (left.type == ValueType::INT && right.type == ValueType::INT) {
        result.type = ValueType::INT;
        result.intVal = left.intVal + right.intVal;
        return result;
    }

    // Float operations
    if (left.type == ValueType::FLOAT || right.type == ValueType::FLOAT) {
        result.type = ValueType::FLOAT;
        double leftVal = (left.type == ValueType::FLOAT) ? left.floatVal : left.intVal.toDouble();
        double rightVal = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();
        result.floatVal = leftVal + rightVal;
        return result;
    }

    return result;
}

Value EvalVisitor::performSubtraction(const Value& left, const Value& right) {
    Value result;

    if (left.type == ValueType::INT && right.type == ValueType::INT) {
        result.type = ValueType::INT;
        result.intVal = left.intVal - right.intVal;
        return result;
    }

    if (left.type == ValueType::FLOAT || right.type == ValueType::FLOAT) {
        result.type = ValueType::FLOAT;
        double leftVal = (left.type == ValueType::FLOAT) ? left.floatVal : left.intVal.toDouble();
        double rightVal = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();
        result.floatVal = leftVal - rightVal;
        return result;
    }

    return result;
}

Value EvalVisitor::performMultiplication(const Value& left, const Value& right) {
    Value result;

    // String repetition
    if (left.type == ValueType::STRING && right.type == ValueType::INT) {
        result.type = ValueType::STRING;
        int count = (int)right.intVal.toDouble();
        for (int i = 0; i < count; ++i) {
            result.strVal += left.strVal;
        }
        return result;
    }

    if (left.type == ValueType::INT && right.type == ValueType::STRING) {
        result.type = ValueType::STRING;
        int count = (int)left.intVal.toDouble();
        for (int i = 0; i < count; ++i) {
            result.strVal += right.strVal;
        }
        return result;
    }

    if (left.type == ValueType::INT && right.type == ValueType::INT) {
        result.type = ValueType::INT;
        result.intVal = left.intVal * right.intVal;
        return result;
    }

    if (left.type == ValueType::FLOAT || right.type == ValueType::FLOAT) {
        result.type = ValueType::FLOAT;
        double leftVal = (left.type == ValueType::FLOAT) ? left.floatVal : left.intVal.toDouble();
        double rightVal = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();
        result.floatVal = leftVal * rightVal;
        return result;
    }

    return result;
}

Value EvalVisitor::performDivision(const Value& left, const Value& right) {
    Value result;
    result.type = ValueType::FLOAT;

    double leftVal = (left.type == ValueType::FLOAT) ? left.floatVal : left.intVal.toDouble();
    double rightVal = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();
    result.floatVal = leftVal / rightVal;

    return result;
}

Value EvalVisitor::performFloorDivision(const Value& left, const Value& right) {
    Value result;

    if (left.type == ValueType::INT && right.type == ValueType::INT) {
        result.type = ValueType::INT;
        result.intVal = left.intVal / right.intVal;
        return result;
    }

    result.type = ValueType::FLOAT;
    double leftVal = (left.type == ValueType::FLOAT) ? left.floatVal : left.intVal.toDouble();
    double rightVal = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();
    result.floatVal = std::floor(leftVal / rightVal);

    return result;
}

Value EvalVisitor::performModulo(const Value& left, const Value& right) {
    Value result;

    if (left.type == ValueType::INT && right.type == ValueType::INT) {
        result.type = ValueType::INT;
        result.intVal = left.intVal % right.intVal;
        return result;
    }

    result.type = ValueType::FLOAT;
    double leftVal = (left.type == ValueType::FLOAT) ? left.floatVal : left.intVal.toDouble();
    double rightVal = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();
    result.floatVal = leftVal - std::floor(leftVal / rightVal) * rightVal;

    return result;
}

Value EvalVisitor::performComparison(const Value& left, const Value& right, const std::string& op) {
    Value result;
    result.type = ValueType::BOOL;

    // String comparison
    if (left.type == ValueType::STRING && right.type == ValueType::STRING) {
        if (op == "<") result.boolVal = left.strVal < right.strVal;
        else if (op == ">") result.boolVal = left.strVal > right.strVal;
        else if (op == "<=") result.boolVal = left.strVal <= right.strVal;
        else if (op == ">=") result.boolVal = left.strVal >= right.strVal;
        else if (op == "==") result.boolVal = left.strVal == right.strVal;
        else if (op == "!=") result.boolVal = left.strVal != right.strVal;
        return result;
    }

    // Type equality
    if (op == "==" || op == "!=") {
        bool equal = false;
        if (left.type == right.type) {
            if (left.type == ValueType::NONE) equal = true;
            else if (left.type == ValueType::BOOL) equal = left.boolVal == right.boolVal;
            else if (left.type == ValueType::INT) equal = left.intVal == right.intVal;
            else if (left.type == ValueType::FLOAT) equal = left.floatVal == right.floatVal;
            else if (left.type == ValueType::STRING) equal = left.strVal == right.strVal;
        } else if ((left.type == ValueType::INT || left.type == ValueType::FLOAT) &&
                   (right.type == ValueType::INT || right.type == ValueType::FLOAT)) {
            double leftVal = (left.type == ValueType::FLOAT) ? left.floatVal : left.intVal.toDouble();
            double rightVal = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();
            equal = leftVal == rightVal;
        }
        result.boolVal = (op == "==") ? equal : !equal;
        return result;
    }

    // Numeric comparison
    if ((left.type == ValueType::INT || left.type == ValueType::FLOAT) &&
        (right.type == ValueType::INT || right.type == ValueType::FLOAT)) {
        if (left.type == ValueType::INT && right.type == ValueType::INT) {
            if (op == "<") result.boolVal = left.intVal < right.intVal;
            else if (op == ">") result.boolVal = left.intVal > right.intVal;
            else if (op == "<=") result.boolVal = left.intVal <= right.intVal;
            else if (op == ">=") result.boolVal = left.intVal >= right.intVal;
        } else {
            double leftVal = (left.type == ValueType::FLOAT) ? left.floatVal : left.intVal.toDouble();
            double rightVal = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();
            if (op == "<") result.boolVal = leftVal < rightVal;
            else if (op == ">") result.boolVal = leftVal > rightVal;
            else if (op == "<=") result.boolVal = leftVal <= rightVal;
            else if (op == ">=") result.boolVal = leftVal >= rightVal;
        }
        return result;
    }

    result.boolVal = false;
    return result;
}

Value EvalVisitor::callBuiltinFunction(const std::string& name, const std::vector<Value>& args) {
    Value result;

    if (name == "print") {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            if (args[i].type == ValueType::STRING) {
                std::cout << args[i].strVal;
            } else {
                std::cout << args[i].toString();
            }
        }
        std::cout << std::endl;
        return result;
    }

    if (args.empty()) return result;

    if (name == "int") {
        return convertToInt(args[0]);
    } else if (name == "float") {
        return convertToFloat(args[0]);
    } else if (name == "str") {
        return convertToString(args[0]);
    } else if (name == "bool") {
        return convertToBool(args[0]);
    }

    return result;
}

Value EvalVisitor::callUserFunction(const Value& func, const std::vector<Value>& args,
                                    const std::map<std::string, Value>& kwargs) {
    enterScope();

    // Bind positional parameters
    for (size_t i = 0; i < func.params.size() && i < args.size(); ++i) {
        setVariable(func.params[i], args[i]);
    }

    // Bind default parameters
    for (const auto& [paramName, defaultValue] : func.defaultParams) {
        if (kwargs.find(paramName) != kwargs.end()) {
            setVariable(paramName, kwargs.at(paramName));
        } else {
            setVariable(paramName, defaultValue);
        }
    }

    // Bind keyword arguments
    for (const auto& [key, value] : kwargs) {
        setVariable(key, value);
    }

    Value result;
    try {
        visit(func.funcBody);
    } catch (ReturnException& e) {
        result = e.value;
    }

    exitScope();
    return result;
}

std::string EvalVisitor::evaluateFormatString(const std::string& fmtStr) {
    std::string result;
    size_t i = 0;

    while (i < fmtStr.length()) {
        if (fmtStr[i] == '{') {
            if (i + 1 < fmtStr.length() && fmtStr[i + 1] == '{') {
                result += '{';
                i += 2;
            } else {
                // Find matching }
                size_t j = i + 1;
                int depth = 1;
                while (j < fmtStr.length() && depth > 0) {
                    if (fmtStr[j] == '{') depth++;
                    else if (fmtStr[j] == '}') depth--;
                    j++;
                }

                std::string expr = fmtStr.substr(i + 1, j - i - 2);

                // Parse and evaluate the expression
                try {
                    ANTLRInputStream input(expr);
                    Python3Lexer lexer(&input);
                    CommonTokenStream tokens(&lexer);
                    tokens.fill();
                    Python3Parser parser(&tokens);
                    tree::ParseTree *tree = parser.test();
                    Value val = std::any_cast<Value>(visit(tree));

                    if (val.type == ValueType::STRING) {
                        result += val.strVal;
                    } else {
                        result += val.toString();
                    }
                } catch (...) {
                    result += expr;
                }

                i = j;
            }
        } else if (fmtStr[i] == '}') {
            if (i + 1 < fmtStr.length() && fmtStr[i + 1] == '}') {
                result += '}';
                i += 2;
            } else {
                result += fmtStr[i];
                i++;
            }
        } else {
            result += fmtStr[i];
            i++;
        }
    }

    return result;
}

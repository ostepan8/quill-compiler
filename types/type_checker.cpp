#include "../include/type_checker.h"
#include "../include/ast.h"
#include <sstream>
#include <algorithm>
#include <iostream>

using namespace quill;

// InferenceContext Implementation
void InferenceContext::setVariableType(const std::string& name, std::unique_ptr<Type> type) {
    variable_types[name] = std::move(type);
}

Type* InferenceContext::getVariableType(const std::string& name) const {
    auto it = variable_types.find(name);
    return (it != variable_types.end()) ? it->second.get() : nullptr;
}

bool InferenceContext::isVariableModified(const std::string& name) const {
    return modified_variables.find(name) != modified_variables.end();
}

void InferenceContext::markVariableModified(const std::string& name) {
    modified_variables.insert(name);
}

std::unique_ptr<InferenceContext> InferenceContext::clone() const {
    auto cloned = std::make_unique<InferenceContext>();
    for (const auto& pair : variable_types) {
        cloned->variable_types[pair.first] = std::unique_ptr<Type>(pair.second->clone());
    }
    cloned->modified_variables = modified_variables;
    return cloned;
}

void InferenceContext::merge(const InferenceContext& other) {
    // Merge modified variables
    for (const auto& var : other.modified_variables) {
        modified_variables.insert(var);
    }
    
    // For type merging, we need to find common types or create unions
    for (const auto& pair : other.variable_types) {
        const std::string& name = pair.first;
        Type* other_type = pair.second.get();
        
        auto it = variable_types.find(name);
        if (it == variable_types.end()) {
            // Variable only exists in other context
            variable_types[name] = std::unique_ptr<Type>(other_type->clone());
        } else {
            // Variable exists in both contexts - find common type
            Type* this_type = it->second.get();
            auto common = TypeFactory::unifyTypes(this_type, other_type);
            if (!common->isError()) {
                variable_types[name] = std::move(common);
            }
        }
    }
}

// TypeChecker Implementation
TypeChecker::TypeChecker() {
    current_context = std::make_unique<InferenceContext>();
    initializeBuiltins();
}

void TypeChecker::initializeBuiltins() {
    // print function: (any) -> void
    std::vector<std::unique_ptr<Type>> print_params;
    print_params.push_back(TypeFactory::createUnknown());
    auto print_type = TypeFactory::createFunction(std::move(print_params), 
                                                 TypeFactory::createVoid());
    defineFunction("print", std::move(print_type));
}

TypeCheckResult TypeChecker::checkProgram(ProgramAST* program) {
    if (!program) {
        TypeCheckResult result;
        result.addError("Null program AST");
        return result;
    }
    
    clearMessages();
    beginInference();
    
    // First pass: collect all function signatures
    for (const auto& func : program->functions) {
        std::vector<std::unique_ptr<Type>> param_types;
        for (const auto& param : func->args) {
            // Assume unknown types - will be inferred
            param_types.push_back(TypeFactory::createUnknown());
        }
        
        auto func_type = TypeFactory::createFunction(std::move(param_types),
                                                   TypeFactory::createUnknown());
        defineFunction(func->name, std::move(func_type));
    }
    
    // Second pass: type check each function
    for (const auto& func : program->functions) {
        auto result = checkFunction(func.get());
        if (result.hasErrors()) {
            for (const auto& error : result.errors) {
                reportError(error);
            }
        }
    }
    
    endInference();
    
    TypeCheckResult result(TypeFactory::createVoid());
    result.errors = error_messages;
    result.warnings = warning_messages;
    
    return result;
}

TypeCheckResult TypeChecker::checkFunction(FunctionAST* function) {
    if (!function) {
        TypeCheckResult result;
        result.addError("Null function AST");
        return result;
    }
    
    pushScope();
    
    // Define parameters in function scope
    for (const auto& param_name : function->args) {
        // Since current AST doesn't have type annotations, default to double
        // (Quill is primarily a numerical language)
        auto param_type = TypeFactory::createFloat();
        defineVariable(param_name, std::move(param_type));
    }
    
    // Check function body
    auto body_result = checkStatement(function->body.get());
    
    popScope();
    
    TypeCheckResult result;
    if (body_result.hasErrors()) {
        result.errors = body_result.errors;
    }
    
    // Infer return type from body
    if (body_result.type) {
        result.type = std::unique_ptr<Type>(body_result.type->clone());
    } else {
        result.type = TypeFactory::createVoid();
    }
    
    return result;
}

TypeCheckResult TypeChecker::checkStatement(StmtAST* stmt) {
    if (!stmt) {
        TypeCheckResult result;
        result.addError("Null statement AST");
        return result;
    }
    
    // Dispatch to specific statement types
    if (auto assign = dynamic_cast<AssignmentStmtAST*>(stmt)) {
        return checkAssignment(assign);
    } else if (auto ret = dynamic_cast<ReturnStmtAST*>(stmt)) {
        return checkReturn(ret);
    } else if (auto if_stmt = dynamic_cast<IfStmtAST*>(stmt)) {
        return checkIf(if_stmt);
    } else if (auto while_stmt = dynamic_cast<WhileStmtAST*>(stmt)) {
        return checkWhile(while_stmt);
    } else if (auto print_stmt = dynamic_cast<PrintStmtAST*>(stmt)) {
        return checkPrint(print_stmt);
    } else if (auto block = dynamic_cast<BlockStmtAST*>(stmt)) {
        return checkBlock(block);
    } else if (auto expr_stmt = dynamic_cast<ExprStmtAST*>(stmt)) {
        return checkExpression(expr_stmt->expression.get());
    }
    
    TypeCheckResult result;
    result.addError("Unknown statement type");
    return result;
}

TypeCheckResult TypeChecker::checkAssignment(const AssignmentStmtAST* stmt) {
    if (!stmt) {
        TypeCheckResult result;
        result.addError("Null assignment statement");
        return result;
    }
    
    // Infer type from expression
    auto expr_result = inferExpressionType(stmt->value.get());
    if (expr_result.hasErrors()) {
        return expr_result;
    }
    
    // Check if variable exists
    Type* existing_type = lookupVariable(stmt->name);
    if (existing_type) {
        // Check assignment compatibility
        if (!isAssignable(existing_type, expr_result.type.get())) {
            TypeCheckResult result;
            result.addError(TypeErrorReporter::formatTypeError(
                "assignment to variable '" + stmt->name + "'",
                existing_type, expr_result.type.get()));
            return result;
        }
    } else {
        // New variable - define it
        defineVariable(stmt->name, std::unique_ptr<Type>(expr_result.type->clone()));
        current_context->setVariableType(stmt->name, 
                                       std::unique_ptr<Type>(expr_result.type->clone()));
    }
    
    current_context->markVariableModified(stmt->name);
    
    return TypeCheckResult(TypeFactory::createVoid());
}

TypeCheckResult TypeChecker::checkReturn(const ReturnStmtAST* stmt) {
    if (!stmt) {
        TypeCheckResult result;
        result.addError("Null return statement");
        return result;
    }
    
    if (stmt->value) {
        return inferExpressionType(stmt->value.get());
    } else {
        return TypeCheckResult(TypeFactory::createVoid());
    }
}

TypeCheckResult TypeChecker::checkIf(const IfStmtAST* stmt) {
    if (!stmt) {
        TypeCheckResult result;
        result.addError("Null if statement");
        return result;
    }
    
    // Check condition type
    auto cond_result = inferExpressionType(stmt->condition.get());
    if (cond_result.hasErrors()) {
        return cond_result;
    }
    
    // Condition should be boolean-compatible
    if (!cond_result.type->isBool() && !cond_result.type->isNumeric()) {
        TypeCheckResult result;
        result.addError("If condition must be boolean or numeric, got: " + 
                       cond_result.type->toString());
        return result;
    }
    
    // Check then branch
    auto then_context = current_context->clone();
    auto then_result = checkStatement(stmt->then_stmt.get());
    
    TypeCheckResult result;
    
    // Check else branch if present
    if (stmt->else_stmt) {
        auto else_context = current_context->clone();
        auto else_result = checkStatement(stmt->else_stmt.get());
        
        if (else_result.hasErrors()) {
            result.errors.insert(result.errors.end(), 
                               else_result.errors.begin(), 
                               else_result.errors.end());
        }
        
        // Merge contexts from both branches
        then_context->merge(*else_context);
    }
    
    if (then_result.hasErrors()) {
        result.errors.insert(result.errors.end(), 
                           then_result.errors.begin(), 
                           then_result.errors.end());
    }
    
    // Update current context with merged information
    current_context = std::move(then_context);
    
    result.type = TypeFactory::createVoid();
    return result;
}

TypeCheckResult TypeChecker::checkWhile(const WhileStmtAST* stmt) {
    if (!stmt) {
        TypeCheckResult result;
        result.addError("Null while statement");
        return result;
    }
    
    // Check condition type
    auto cond_result = inferExpressionType(stmt->condition.get());
    if (cond_result.hasErrors()) {
        return cond_result;
    }
    
    // Condition should be boolean-compatible
    if (!cond_result.type->isBool() && !cond_result.type->isNumeric()) {
        TypeCheckResult result;
        result.addError("While condition must be boolean or numeric, got: " + 
                       cond_result.type->toString());
        return result;
    }
    
    // Check body in a loop context
    auto body_result = checkStatement(stmt->body.get());
    
    TypeCheckResult result;
    if (body_result.hasErrors()) {
        result.errors = body_result.errors;
    }
    
    result.type = TypeFactory::createVoid();
    return result;
}

TypeCheckResult TypeChecker::checkPrint(const PrintStmtAST* stmt) {
    if (!stmt) {
        TypeCheckResult result;
        result.addError("Null print statement");
        return result;
    }
    
    auto expr_result = inferExpressionType(stmt->expression.get());
    if (expr_result.hasErrors()) {
        return expr_result;
    }
    
    // Print accepts any type
    return TypeCheckResult(TypeFactory::createVoid());
}

TypeCheckResult TypeChecker::checkBlock(const BlockStmtAST* stmt) {
    if (!stmt) {
        TypeCheckResult result;
        result.addError("Null block statement");
        return result;
    }
    
    pushScope();
    
    TypeCheckResult result(TypeFactory::createVoid());
    
    for (const auto& statement : stmt->statements) {
        auto stmt_result = checkStatement(statement.get());
        if (stmt_result.hasErrors()) {
            result.errors.insert(result.errors.end(),
                               stmt_result.errors.begin(),
                               stmt_result.errors.end());
        }
        
        // Update result type with last non-void statement
        if (stmt_result.type && !stmt_result.type->isVoid()) {
            result.type = std::unique_ptr<Type>(stmt_result.type->clone());
        }
    }
    
    popScope();
    return result;
}

TypeCheckResult TypeChecker::inferExpressionType(ExprAST* expr) {
    if (!expr) {
        TypeCheckResult result;
        result.addError("Null expression");
        return result;
    }
    
    if (auto num = dynamic_cast<NumberExprAST*>(expr)) {
        return inferNumberType(num);
    } else if (auto str = dynamic_cast<StringExprAST*>(expr)) {
        return inferStringType(str);
    } else if (auto var = dynamic_cast<VariableExprAST*>(expr)) {
        return inferVariableType(var);
    } else if (auto bin = dynamic_cast<BinaryExprAST*>(expr)) {
        return inferBinaryType(bin);
    } else if (auto unary = dynamic_cast<UnaryExprAST*>(expr)) {
        return inferUnaryType(unary);
    } else if (auto call = dynamic_cast<CallExprAST*>(expr)) {
        return inferCallType(call);
    }
    
    TypeCheckResult result;
    result.addError("Unknown expression type");
    return result;
}

TypeCheckResult TypeChecker::checkExpression(ExprAST* expr) {
    // checkExpression wraps inferExpressionType
    return inferExpressionType(expr);
}

TypeCheckResult TypeChecker::inferNumberType(NumberExprAST* expr) {
    if (!expr) {
        TypeCheckResult result;
        result.addError("Null number expression");
        return result;
    }
    
    // Determine if it's an integer or float based on the value
    if (expr->value == static_cast<int>(expr->value)) {
        return TypeCheckResult(TypeFactory::createInt());
    } else {
        return TypeCheckResult(TypeFactory::createFloat());
    }
}

TypeCheckResult TypeChecker::inferStringType(StringExprAST* expr) {
    if (!expr) {
        TypeCheckResult result;
        result.addError("Null string expression");
        return result;
    }
    
    return TypeCheckResult(TypeFactory::createString());
}

TypeCheckResult TypeChecker::inferVariableType(VariableExprAST* expr) {
    if (!expr) {
        TypeCheckResult result;
        result.addError("Null variable expression");
        return result;
    }
    
    Type* type = lookupVariable(expr->name);
    if (!type) {
        TypeCheckResult result;
        result.addError(TypeErrorReporter::formatUndefinedVariable(expr->name));
        return result;
    }
    
    return TypeCheckResult(std::unique_ptr<Type>(type->clone()));
}

TypeCheckResult TypeChecker::inferBinaryType(BinaryExprAST* expr) {
    if (!expr) {
        TypeCheckResult result;
        result.addError("Null binary expression");
        return result;
    }
    
    auto left_result = inferExpressionType(expr->lhs.get());
    if (left_result.hasErrors()) {
        return left_result;
    }
    
    auto right_result = inferExpressionType(expr->rhs.get());
    if (right_result.hasErrors()) {
        return right_result;
    }
    
    Type* left_type = left_result.type.get();
    Type* right_type = right_result.type.get();
    
    switch (expr->op) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
            // Arithmetic operations
            if (left_type->isNumeric() && right_type->isNumeric()) {
                return TypeCheckResult(TypeFactory::promoteNumericTypes(left_type, right_type));
            } else {
                TypeCheckResult result;
                result.addError("Arithmetic operation requires numeric types, got: " +
                               left_type->toString() + " " + expr->op + " " + right_type->toString());
                return result;
            }
            
        case '<':
        case '>':
        case '=': // ==
        case '!': // !=
            // Comparison operations
            if (isComparable(left_type, right_type)) {
                return TypeCheckResult(TypeFactory::createBool());
            } else {
                TypeCheckResult result;
                result.addError("Cannot compare incompatible types: " +
                               left_type->toString() + " and " + right_type->toString());
                return result;
            }
            
        default:
            TypeCheckResult result;
            result.addError("Unknown binary operator: " + std::string(1, expr->op));
            return result;
    }
}

TypeCheckResult TypeChecker::inferUnaryType(UnaryExprAST* expr) {
    if (!expr) {
        TypeCheckResult result;
        result.addError("Null unary expression");
        return result;
    }
    
    auto operand_result = inferExpressionType(expr->operand.get());
    if (operand_result.hasErrors()) {
        return operand_result;
    }
    
    Type* operand_type = operand_result.type.get();
    
    switch (expr->op) {
        case '-':
            if (operand_type->isNumeric()) {
                return TypeCheckResult(std::unique_ptr<Type>(operand_type->clone()));
            } else {
                TypeCheckResult result;
                result.addError("Unary minus requires numeric type, got: " + operand_type->toString());
                return result;
            }
            
        case '!':
            // Logical not
            return TypeCheckResult(TypeFactory::createBool());
            
        default:
            TypeCheckResult result;
            result.addError("Unknown unary operator: " + std::string(1, expr->op));
            return result;
    }
}

TypeCheckResult TypeChecker::inferCallType(CallExprAST* expr) {
    if (!expr) {
        TypeCheckResult result;
        result.addError("Null call expression");
        return result;
    }
    
    // Infer argument types
    std::vector<Type*> arg_types;
    std::vector<TypeCheckResult> arg_results;
    
    for (const auto& arg : expr->args) {
        auto arg_result = inferExpressionType(arg.get());
        if (arg_result.hasErrors()) {
            return arg_result;
        }
        arg_types.push_back(arg_result.type.get());
        arg_results.push_back(std::move(arg_result));
    }
    
    // Look up function
    FunctionType* func_type = lookupFunction(expr->callee, arg_types);
    if (!func_type) {
        TypeCheckResult result;
        result.addError(TypeErrorReporter::formatUndefinedFunction(expr->callee));
        return result;
    }
    
    return TypeCheckResult(std::unique_ptr<Type>(func_type->return_type->clone()));
}

bool TypeChecker::isAssignable(const Type* target, const Type* source) {
    if (!target || !source) return false;
    return target->isAssignableFrom(source);
}

bool TypeChecker::isComparable(const Type* left, const Type* right) {
    if (!left || !right) return false;
    
    // Same types are comparable
    if (left->equals(right)) return true;
    
    // Numeric types are comparable with each other
    if (left->isNumeric() && right->isNumeric()) return true;
    
    // String comparisons
    if (left->isString() && right->isString()) return true;
    
    return false;
}

void TypeChecker::pushScope() {
    type_env.pushScope();
}

void TypeChecker::popScope() {
    type_env.popScope();
}

void TypeChecker::defineVariable(const std::string& name, std::unique_ptr<Type> type) {
    type_env.define(name, std::move(type));
}

void TypeChecker::defineFunction(const std::string& name, std::unique_ptr<FunctionType> type) {
    type_env.defineFunction(name, std::move(type));
}

Type* TypeChecker::lookupVariable(const std::string& name) {
    // First check inference context
    if (current_context) {
        Type* context_type = current_context->getVariableType(name);
        if (context_type) {
            return context_type;
        }
    }
    
    // Then check type environment
    return type_env.lookup(name);
}

FunctionType* TypeChecker::lookupFunction(const std::string& name, const std::vector<Type*>& arg_types) {
    return type_env.lookupFunction(name, arg_types);
}

void TypeChecker::beginInference() {
    current_context = std::make_unique<InferenceContext>();
}

void TypeChecker::endInference() {
    current_context.reset();
}

void TypeChecker::reportError(const std::string& message) {
    error_messages.push_back(message);
}

void TypeChecker::reportWarning(const std::string& message) {
    warning_messages.push_back(message);
}

void TypeChecker::clearMessages() {
    error_messages.clear();
    warning_messages.clear();
}

// TypeErrorReporter Implementation
std::string TypeErrorReporter::formatTypeError(const std::string& context, 
                                             const Type* expected, 
                                             const Type* actual) {
    std::stringstream ss;
    ss << "Type error in " << context << ": expected " 
       << (expected ? expected->toString() : "unknown")
       << ", got " 
       << (actual ? actual->toString() : "unknown");
    return ss.str();
}

std::string TypeErrorReporter::formatIncompatibleTypes(const Type* left, const Type* right) {
    std::stringstream ss;
    ss << "Incompatible types: " 
       << (left ? left->toString() : "unknown")
       << " and " 
       << (right ? right->toString() : "unknown");
    return ss.str();
}

std::string TypeErrorReporter::formatUndefinedVariable(const std::string& name) {
    return "Undefined variable: " + name;
}

std::string TypeErrorReporter::formatUndefinedFunction(const std::string& name) {
    return "Undefined function: " + name;
}

std::string TypeErrorReporter::formatArgumentMismatch(const std::string& function_name,
                                                    size_t expected_count,
                                                    size_t actual_count) {
    std::stringstream ss;
    ss << "Function " << function_name << " expects " << expected_count 
       << " arguments, got " << actual_count;
    return ss.str();
}

// TypeAnnotationResolver Implementation
std::unique_ptr<Type> TypeAnnotationResolver::resolveAnnotation(const std::string& annotation) {
    if (annotation.empty()) {
        return TypeFactory::createUnknown();
    }
    
    // Simple type name resolution
    if (annotation == "int") {
        return TypeFactory::createInt();
    } else if (annotation == "float") {
        return TypeFactory::createFloat();
    } else if (annotation == "bool") {
        return TypeFactory::createBool();
    } else if (annotation == "str" || annotation == "string") {
        return TypeFactory::createString();
    } else if (annotation == "void") {
        return TypeFactory::createVoid();
    }
    
    // Check for list types: list[T]
    if (annotation.substr(0, 5) == "list[" && annotation.back() == ']') {
        std::string element_type = annotation.substr(5, annotation.length() - 6);
        auto elem_type = resolveAnnotation(element_type);
        if (elem_type->isError()) {
            return elem_type;
        }
        return TypeFactory::createList(std::move(elem_type));
    }
    
    // Check for tuple types: tuple[T1, T2, ...]
    if (annotation.substr(0, 6) == "tuple[" && annotation.back() == ']') {
        std::string types_str = annotation.substr(6, annotation.length() - 7);
        // Simple comma parsing (can be improved later)
        std::vector<std::unique_ptr<Type>> element_types;
        
        std::stringstream ss(types_str);
        std::string type_str;
        while (std::getline(ss, type_str, ',')) {
            // Trim whitespace
            type_str.erase(0, type_str.find_first_not_of(" \t"));
            type_str.erase(type_str.find_last_not_of(" \t") + 1);
            
            auto elem_type = resolveAnnotation(type_str);
            if (elem_type->isError()) {
                return elem_type;
            }
            element_types.push_back(std::move(elem_type));
        }
        
        return TypeFactory::createTuple(std::move(element_types));
    }
    
    // Check for union types: T1 | T2 | ...
    size_t union_pos = annotation.find(" | ");
    if (union_pos != std::string::npos) {
        std::vector<std::unique_ptr<Type>> union_types;
        
        std::stringstream ss(annotation);
        std::string type_str;
        while (std::getline(ss, type_str, '|')) {
            // Trim whitespace
            type_str.erase(0, type_str.find_first_not_of(" \t"));
            type_str.erase(type_str.find_last_not_of(" \t") + 1);
            
            auto union_type = resolveAnnotation(type_str);
            if (union_type->isError()) {
                return union_type;
            }
            union_types.push_back(std::move(union_type));
        }
        
        return TypeFactory::createUnion(std::move(union_types));
    }
    
    return TypeFactory::createError("Unknown type annotation: " + annotation);
}

std::unique_ptr<Type> TypeAnnotationResolver::resolveFromFunction(const FunctionAST* func) {
    if (!func) {
        return TypeFactory::createError("Null function");
    }
    
    std::vector<std::unique_ptr<Type>> param_types;
    for (const auto& param_name : func->args) {
        // Default to double for all parameters (simplified type system)
        auto param_type = TypeFactory::createFloat();
        param_types.push_back(std::move(param_type));
    }
    
    // Since current AST doesn't have return type annotations, 
    // default to double (most Quill functions return double)
    auto return_type = TypeFactory::createFloat();
    
    return TypeFactory::createFunction(std::move(param_types), std::move(return_type));
}

bool TypeAnnotationResolver::validateTypeAnnotation(const std::string& annotation) {
    auto resolved = resolveAnnotation(annotation);
    return !resolved->isError();
}
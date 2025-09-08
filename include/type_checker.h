#pragma once
#include "type_system.h"
#include "ast.h"
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace quill {

// Type inference context for flow-sensitive analysis
class InferenceContext {
public:
    std::map<std::string, std::unique_ptr<Type>> variable_types;
    std::set<std::string> modified_variables;
    
    void setVariableType(const std::string& name, std::unique_ptr<Type> type);
    Type* getVariableType(const std::string& name) const;
    bool isVariableModified(const std::string& name) const;
    void markVariableModified(const std::string& name);
    
    // Create a copy of the context for branching control flow
    std::unique_ptr<InferenceContext> clone() const;
    
    // Merge contexts from different control flow branches
    void merge(const InferenceContext& other);
};

// Type checking result
struct TypeCheckResult {
    std::unique_ptr<Type> type;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    bool hasErrors() const { return !errors.empty(); }
    void addError(const std::string& error) { errors.push_back(error); }
    void addWarning(const std::string& warning) { warnings.push_back(warning); }
    
    TypeCheckResult(std::unique_ptr<Type> t = nullptr) : type(std::move(t)) {}
};

// Main type checker class
class TypeChecker {
private:
    TypeEnvironment type_env;
    std::unique_ptr<InferenceContext> current_context;
    std::vector<std::string> error_messages;
    std::vector<std::string> warning_messages;
    
    // Built-in functions
    void initializeBuiltins();
    
    // Internal helper functions are implemented in the .cpp file
    
    // Control flow analysis
    void analyzeControlFlow(const IfStmtAST* stmt, InferenceContext& context);
    void analyzeControlFlow(const WhileStmtAST* stmt, InferenceContext& context);
    
    // Error reporting
    void reportError(const std::string& message);
    void reportWarning(const std::string& message);
    
public:
    TypeChecker();
    ~TypeChecker() = default;
    
    // Main type checking interface
    TypeCheckResult checkProgram(ProgramAST* program);
    TypeCheckResult checkFunction(FunctionAST* function);
    TypeCheckResult checkStatement(StmtAST* stmt);
    TypeCheckResult checkExpression(ExprAST* expr);
    
    // Specific statement checking
    TypeCheckResult checkAssignment(const AssignmentStmtAST* stmt);
    TypeCheckResult checkReturn(const ReturnStmtAST* stmt);
    TypeCheckResult checkIf(const IfStmtAST* stmt);
    TypeCheckResult checkWhile(const WhileStmtAST* stmt);
    TypeCheckResult checkPrint(const PrintStmtAST* stmt);
    TypeCheckResult checkBlock(const BlockStmtAST* stmt);
    
    // Type inference for expressions
    TypeCheckResult inferExpressionType(ExprAST* expr);
    TypeCheckResult inferNumberType(NumberExprAST* expr);
    TypeCheckResult inferStringType(StringExprAST* expr);
    TypeCheckResult inferVariableType(VariableExprAST* expr);
    TypeCheckResult inferBinaryType(BinaryExprAST* expr);
    TypeCheckResult inferUnaryType(UnaryExprAST* expr);
    TypeCheckResult inferCallType(CallExprAST* expr);
    
    // Type compatibility checking
    bool isAssignable(const Type* target, const Type* source);
    bool isComparable(const Type* left, const Type* right);
    std::unique_ptr<Type> getCommonType(const Type* left, const Type* right);
    
    // Scope management
    void pushScope();
    void popScope();
    void defineVariable(const std::string& name, std::unique_ptr<Type> type);
    void defineFunction(const std::string& name, std::unique_ptr<FunctionType> type);
    
    // Error and warning access
    const std::vector<std::string>& getErrors() const { return error_messages; }
    const std::vector<std::string>& getWarnings() const { return warning_messages; }
    void clearMessages();
    
    // Type information access
    Type* lookupVariable(const std::string& name);
    FunctionType* lookupFunction(const std::string& name, const std::vector<Type*>& arg_types);
    
    // Flow-sensitive type analysis
    void beginInference();
    void endInference();
    InferenceContext* getCurrentContext() { return current_context.get(); }
};

// Type annotation support for explicit type declarations
class TypeAnnotationResolver {
public:
    static std::unique_ptr<Type> resolveAnnotation(const std::string& annotation);
    static std::unique_ptr<Type> resolveFromFunction(const FunctionAST* func);
    static bool validateTypeAnnotation(const std::string& annotation);
};

// Type error reporting utilities
class TypeErrorReporter {
public:
    static std::string formatTypeError(const std::string& context, 
                                     const Type* expected, 
                                     const Type* actual);
    static std::string formatIncompatibleTypes(const Type* left, const Type* right);
    static std::string formatUndefinedVariable(const std::string& name);
    static std::string formatUndefinedFunction(const std::string& name);
    static std::string formatArgumentMismatch(const std::string& function_name,
                                            size_t expected_count,
                                            size_t actual_count);
};

} // namespace quill
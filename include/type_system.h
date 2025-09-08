#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <optional>

namespace quill {

// Forward declarations
class Type;
class TypeChecker;

// Type kind enumeration
enum class TypeKind {
    // Primitive types
    INT,
    FLOAT, 
    BOOL,
    STRING,
    VOID,
    
    // Composite types
    FUNCTION,
    LIST,
    TUPLE,
    
    // Advanced types
    GENERIC,
    UNION,
    DISCRIMINATED_UNION,
    INTERFACE,
    UNKNOWN,
    ERROR_TYPE
};

// Base Type class
class Type {
public:
    TypeKind kind;
    std::string name;
    
    Type(TypeKind k, const std::string& n) : kind(k), name(n) {}
    virtual ~Type() = default;
    
    virtual std::string toString() const { return name; }
    virtual bool equals(const Type* other) const;
    virtual bool isAssignableFrom(const Type* other) const;
    virtual Type* clone() const = 0;
    
    // Type predicates
    bool isPrimitive() const;
    bool isNumeric() const;
    bool isInteger() const { return kind == TypeKind::INT; }
    bool isFloat() const { return kind == TypeKind::FLOAT; }
    bool isBool() const { return kind == TypeKind::BOOL; }
    bool isString() const { return kind == TypeKind::STRING; }
    bool isVoid() const { return kind == TypeKind::VOID; }
    bool isFunction() const { return kind == TypeKind::FUNCTION; }
    bool isList() const { return kind == TypeKind::LIST; }
    bool isUnknown() const { return kind == TypeKind::UNKNOWN; }
    bool isError() const { return kind == TypeKind::ERROR_TYPE; }
};

// Primitive Types
class IntType : public Type {
public:
    IntType() : Type(TypeKind::INT, "int") {}
    Type* clone() const override { return new IntType(); }
};

class FloatType : public Type {
public:
    FloatType() : Type(TypeKind::FLOAT, "float") {}
    Type* clone() const override { return new FloatType(); }
    bool isAssignableFrom(const Type* other) const override;
};

class BoolType : public Type {
public:
    BoolType() : Type(TypeKind::BOOL, "bool") {}
    Type* clone() const override { return new BoolType(); }
};

class StringType : public Type {
public:
    StringType() : Type(TypeKind::STRING, "str") {}
    Type* clone() const override { return new StringType(); }
};

class VoidType : public Type {
public:
    VoidType() : Type(TypeKind::VOID, "void") {}
    Type* clone() const override { return new VoidType(); }
};

// Function Type
class FunctionType : public Type {
public:
    std::vector<std::unique_ptr<Type>> param_types;
    std::unique_ptr<Type> return_type;
    
    FunctionType(std::vector<std::unique_ptr<Type>> params, std::unique_ptr<Type> ret);
    
    std::string toString() const override;
    bool equals(const Type* other) const override;
    Type* clone() const override;
};

// List Type
class ListType : public Type {
public:
    std::unique_ptr<Type> element_type;
    
    ListType(std::unique_ptr<Type> elem) 
        : Type(TypeKind::LIST, "list"), element_type(std::move(elem)) {}
    
    std::string toString() const override;
    bool equals(const Type* other) const override;
    Type* clone() const override;
};

// Tuple Type
class TupleType : public Type {
public:
    std::vector<std::unique_ptr<Type>> element_types;
    
    TupleType(std::vector<std::unique_ptr<Type>> elems);
    
    std::string toString() const override;
    bool equals(const Type* other) const override;
    Type* clone() const override;
};

// Union Type
class UnionType : public Type {
public:
    std::vector<std::unique_ptr<Type>> types;
    
    UnionType(std::vector<std::unique_ptr<Type>> union_types);
    
    std::string toString() const override;
    bool equals(const Type* other) const override;
    bool isAssignableFrom(const Type* other) const override;
    Type* clone() const override;
};

// Generic Type Parameter
class GenericType : public Type {
public:
    std::string parameter_name;
    std::vector<std::unique_ptr<Type>> constraints;
    
    GenericType(const std::string& name) 
        : Type(TypeKind::GENERIC, name), parameter_name(name) {}
    
    std::string toString() const override;
    Type* clone() const override;
};

// Special types for type inference
class UnknownType : public Type {
public:
    UnknownType() : Type(TypeKind::UNKNOWN, "unknown") {}
    Type* clone() const override { return new UnknownType(); }
    bool isAssignableFrom(const Type* other) const override { return true; }
};

class ErrorType : public Type {
public:
    std::string error_message;
    
    ErrorType(const std::string& msg = "type error") 
        : Type(TypeKind::ERROR_TYPE, "error"), error_message(msg) {}
    Type* clone() const override { return new ErrorType(error_message); }
};

// Type Environment for scoping
class TypeEnvironment {
private:
    std::vector<std::map<std::string, std::unique_ptr<Type>>> scopes;
    
public:
    TypeEnvironment();
    
    void pushScope();
    void popScope();
    
    void define(const std::string& name, std::unique_ptr<Type> type);
    Type* lookup(const std::string& name) const;
    bool isDefinedInCurrentScope(const std::string& name) const;
    
    // For function overloading
    void defineFunction(const std::string& name, std::unique_ptr<FunctionType> type);
    FunctionType* lookupFunction(const std::string& name, 
                                const std::vector<Type*>& arg_types) const;
};

// Type Factory for common types
class TypeFactory {
public:
    static std::unique_ptr<Type> createInt();
    static std::unique_ptr<Type> createFloat();
    static std::unique_ptr<Type> createBool();
    static std::unique_ptr<Type> createString();
    static std::unique_ptr<Type> createVoid();
    static std::unique_ptr<Type> createUnknown();
    static std::unique_ptr<Type> createError(const std::string& message = "");
    
    static std::unique_ptr<FunctionType> createFunction(
        std::vector<std::unique_ptr<Type>> params,
        std::unique_ptr<Type> return_type
    );
    
    static std::unique_ptr<ListType> createList(std::unique_ptr<Type> element_type);
    static std::unique_ptr<TupleType> createTuple(std::vector<std::unique_ptr<Type>> elements);
    static std::unique_ptr<UnionType> createUnion(std::vector<std::unique_ptr<Type>> types);
    
    // Type promotion and unification
    static std::unique_ptr<Type> promoteNumericTypes(const Type* t1, const Type* t2);
    static std::unique_ptr<Type> unifyTypes(const Type* t1, const Type* t2);
    static std::unique_ptr<Type> getCommonType(const std::vector<Type*>& types);
};

// Generic Type Instantiation Support
class GenericInstantiator {
private:
    std::map<std::string, std::unique_ptr<Type>> type_bindings;
    
public:
    void bindTypeVariable(const std::string& var_name, std::unique_ptr<Type> concrete_type);
    Type* getBinding(const std::string& var_name) const;
    void clearBindings();
    
    std::unique_ptr<Type> instantiate(const Type* generic_type);
    std::unique_ptr<FunctionType> instantiateFunction(const FunctionType* generic_func);
};

// Generic Type Constraint System
class TypeConstraints {
public:
    enum class ConstraintKind {
        EQUALS,           // T = U
        SUBTYPE,          // T <: U  
        IMPLEMENTS,       // T implements Interface
        NUMERIC,          // T is numeric
        COMPARABLE        // T is comparable
    };
    
    struct Constraint {
        ConstraintKind kind;
        std::unique_ptr<Type> left;
        std::unique_ptr<Type> right;
        
        Constraint(ConstraintKind k, std::unique_ptr<Type> l, std::unique_ptr<Type> r = nullptr)
            : kind(k), left(std::move(l)), right(std::move(r)) {}
    };
    
private:
    std::vector<Constraint> constraints;
    
public:
    void addConstraint(ConstraintKind kind, std::unique_ptr<Type> left, 
                      std::unique_ptr<Type> right = nullptr);
    bool solve(GenericInstantiator& instantiator);
    void clear();
    
    const std::vector<Constraint>& getConstraints() const { return constraints; }
};

// Advanced Union Type with Discriminated Unions
class DiscriminatedUnionType : public UnionType {
public:
    struct Variant {
        std::string tag;
        std::unique_ptr<Type> data_type;
        
        Variant(const std::string& t, std::unique_ptr<Type> dt)
            : tag(t), data_type(std::move(dt)) {}
    };
    
private:
    std::vector<Variant> variants;
    
public:
    DiscriminatedUnionType(std::vector<Variant> vars);
    
    const std::vector<Variant>& getVariants() const { return variants; }
    Type* getVariantType(const std::string& tag) const;
    bool hasVariant(const std::string& tag) const;
    
    std::string toString() const override;
    bool equals(const Type* other) const override;
    Type* clone() const override;
};

// Interface/Trait Type for structural typing
class InterfaceType : public Type {
public:
    struct Method {
        std::string name;
        std::unique_ptr<FunctionType> signature;
        
        Method(const std::string& n, std::unique_ptr<FunctionType> sig)
            : name(n), signature(std::move(sig)) {}
    };
    
private:
    std::vector<Method> methods;
    std::string interface_name;
    
public:
    InterfaceType(const std::string& name, std::vector<Method> meths);
    
    std::string toString() const override;
    bool equals(const Type* other) const override;
    Type* clone() const override;
    bool isAssignableFrom(const Type* other) const override;
    
    const std::vector<Method>& getMethods() const { return methods; }
    bool hasMethod(const std::string& name) const;
    const FunctionType* getMethod(const std::string& name) const;
};

// Type Annotation Parser
class TypeAnnotationParser {
public:
    static std::unique_ptr<Type> parseTypeAnnotation(const std::string& annotation);
    static std::unique_ptr<Type> parseFromTokens(const std::vector<std::string>& tokens, size_t& pos);
    
private:
    static std::unique_ptr<Type> parsePrimitiveType(const std::string& name);
    static std::unique_ptr<Type> parseListType(const std::vector<std::string>& tokens, size_t& pos);
    static std::unique_ptr<Type> parseTupleType(const std::vector<std::string>& tokens, size_t& pos);
    static std::unique_ptr<Type> parseUnionType(const std::vector<std::string>& tokens, size_t& pos);
    static std::unique_ptr<Type> parseGenericType(const std::vector<std::string>& tokens, size_t& pos);
    static std::unique_ptr<Type> parseInterfaceType(const std::vector<std::string>& tokens, size_t& pos);
};

} // namespace quill
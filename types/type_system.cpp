#include "../include/type_system.h"
#include <sstream>
#include <algorithm>
#include <iostream>

using namespace quill;

// Type base class implementations
bool Type::equals(const Type* other) const {
    return other && kind == other->kind && name == other->name;
}

bool Type::isAssignableFrom(const Type* other) const {
    return equals(other);
}

bool Type::isPrimitive() const {
    return kind == TypeKind::INT || kind == TypeKind::FLOAT || 
           kind == TypeKind::BOOL || kind == TypeKind::STRING;
}

bool Type::isNumeric() const {
    return kind == TypeKind::INT || kind == TypeKind::FLOAT;
}

// FloatType specific assignment rules
bool FloatType::isAssignableFrom(const Type* other) const {
    // Floats can accept ints (with promotion)
    return other && (other->kind == TypeKind::FLOAT || other->kind == TypeKind::INT);
}

// FunctionType implementation
FunctionType::FunctionType(std::vector<std::unique_ptr<Type>> params, std::unique_ptr<Type> ret)
    : Type(TypeKind::FUNCTION, "function"), return_type(std::move(ret)) {
    for (auto& param : params) {
        param_types.push_back(std::move(param));
    }
}

std::string FunctionType::toString() const {
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << param_types[i]->toString();
    }
    ss << ") -> " << return_type->toString();
    return ss.str();
}

bool FunctionType::equals(const Type* other) const {
    if (!other || other->kind != TypeKind::FUNCTION) return false;
    
    const FunctionType* other_func = static_cast<const FunctionType*>(other);
    
    if (param_types.size() != other_func->param_types.size()) return false;
    
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (!param_types[i]->equals(other_func->param_types[i].get())) {
            return false;
        }
    }
    
    return return_type->equals(other_func->return_type.get());
}

Type* FunctionType::clone() const {
    std::vector<std::unique_ptr<Type>> cloned_params;
    for (const auto& param : param_types) {
        cloned_params.push_back(std::unique_ptr<Type>(param->clone()));
    }
    
    return new FunctionType(std::move(cloned_params), 
                           std::unique_ptr<Type>(return_type->clone()));
}

// ListType implementation
std::string ListType::toString() const {
    return "list[" + element_type->toString() + "]";
}

bool ListType::equals(const Type* other) const {
    if (!other || other->kind != TypeKind::LIST) return false;
    const ListType* other_list = static_cast<const ListType*>(other);
    return element_type->equals(other_list->element_type.get());
}

Type* ListType::clone() const {
    return new ListType(std::unique_ptr<Type>(element_type->clone()));
}

// TupleType implementation
TupleType::TupleType(std::vector<std::unique_ptr<Type>> elems) 
    : Type(TypeKind::TUPLE, "tuple") {
    for (auto& elem : elems) {
        element_types.push_back(std::move(elem));
    }
}

std::string TupleType::toString() const {
    std::stringstream ss;
    ss << "tuple[";
    for (size_t i = 0; i < element_types.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << element_types[i]->toString();
    }
    ss << "]";
    return ss.str();
}

bool TupleType::equals(const Type* other) const {
    if (!other || other->kind != TypeKind::TUPLE) return false;
    
    const TupleType* other_tuple = static_cast<const TupleType*>(other);
    if (element_types.size() != other_tuple->element_types.size()) return false;
    
    for (size_t i = 0; i < element_types.size(); ++i) {
        if (!element_types[i]->equals(other_tuple->element_types[i].get())) {
            return false;
        }
    }
    
    return true;
}

Type* TupleType::clone() const {
    std::vector<std::unique_ptr<Type>> cloned_elements;
    for (const auto& elem : element_types) {
        cloned_elements.push_back(std::unique_ptr<Type>(elem->clone()));
    }
    return new TupleType(std::move(cloned_elements));
}

// UnionType implementation
UnionType::UnionType(std::vector<std::unique_ptr<Type>> union_types) 
    : Type(TypeKind::UNION, "union") {
    for (auto& type : union_types) {
        types.push_back(std::move(type));
    }
}

std::string UnionType::toString() const {
    std::stringstream ss;
    for (size_t i = 0; i < types.size(); ++i) {
        if (i > 0) ss << " | ";
        ss << types[i]->toString();
    }
    return ss.str();
}

bool UnionType::equals(const Type* other) const {
    if (!other || other->kind != TypeKind::UNION) return false;
    
    const UnionType* other_union = static_cast<const UnionType*>(other);
    if (types.size() != other_union->types.size()) return false;
    
    // Check if all types are present (order doesn't matter)
    for (const auto& type : types) {
        bool found = false;
        for (const auto& other_type : other_union->types) {
            if (type->equals(other_type.get())) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    
    return true;
}

bool UnionType::isAssignableFrom(const Type* other) const {
    if (!other) return false;
    
    // Check if other is one of the union types
    for (const auto& type : types) {
        if (type->isAssignableFrom(other)) {
            return true;
        }
    }
    
    return false;
}

Type* UnionType::clone() const {
    std::vector<std::unique_ptr<Type>> cloned_types;
    for (const auto& type : types) {
        cloned_types.push_back(std::unique_ptr<Type>(type->clone()));
    }
    return new UnionType(std::move(cloned_types));
}

// GenericType implementation
std::string GenericType::toString() const {
    return parameter_name;
}

Type* GenericType::clone() const {
    auto cloned = new GenericType(parameter_name);
    for (const auto& constraint : constraints) {
        cloned->constraints.push_back(std::unique_ptr<Type>(constraint->clone()));
    }
    return cloned;
}

// TypeEnvironment implementation
TypeEnvironment::TypeEnvironment() {
    pushScope(); // Global scope
}

void TypeEnvironment::pushScope() {
    scopes.emplace_back();
}

void TypeEnvironment::popScope() {
    if (scopes.size() > 1) { // Keep global scope
        scopes.pop_back();
    }
}

void TypeEnvironment::define(const std::string& name, std::unique_ptr<Type> type) {
    if (!scopes.empty()) {
        scopes.back()[name] = std::move(type);
    }
}

Type* TypeEnvironment::lookup(const std::string& name) const {
    // Search from innermost to outermost scope
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second.get();
        }
    }
    return nullptr;
}

bool TypeEnvironment::isDefinedInCurrentScope(const std::string& name) const {
    if (scopes.empty()) return false;
    return scopes.back().find(name) != scopes.back().end();
}

void TypeEnvironment::defineFunction(const std::string& name, std::unique_ptr<FunctionType> type) {
    define(name, std::move(type));
}

FunctionType* TypeEnvironment::lookupFunction(const std::string& name, 
                                             const std::vector<Type*>& arg_types) const {
    Type* type = lookup(name);
    if (!type || !type->isFunction()) return nullptr;
    
    FunctionType* func_type = static_cast<FunctionType*>(type);
    
    // Check if argument types match
    if (func_type->param_types.size() != arg_types.size()) return nullptr;
    
    for (size_t i = 0; i < arg_types.size(); ++i) {
        if (!func_type->param_types[i]->isAssignableFrom(arg_types[i])) {
            return nullptr;
        }
    }
    
    return func_type;
}

// TypeFactory implementation
std::unique_ptr<Type> TypeFactory::createInt() {
    return std::make_unique<IntType>();
}

std::unique_ptr<Type> TypeFactory::createFloat() {
    return std::make_unique<FloatType>();
}

std::unique_ptr<Type> TypeFactory::createBool() {
    return std::make_unique<BoolType>();
}

std::unique_ptr<Type> TypeFactory::createString() {
    return std::make_unique<StringType>();
}

std::unique_ptr<Type> TypeFactory::createVoid() {
    return std::make_unique<VoidType>();
}

std::unique_ptr<Type> TypeFactory::createUnknown() {
    return std::make_unique<UnknownType>();
}

std::unique_ptr<Type> TypeFactory::createError(const std::string& message) {
    return std::make_unique<ErrorType>(message);
}

std::unique_ptr<FunctionType> TypeFactory::createFunction(
    std::vector<std::unique_ptr<Type>> params,
    std::unique_ptr<Type> return_type) {
    return std::make_unique<FunctionType>(std::move(params), std::move(return_type));
}

std::unique_ptr<ListType> TypeFactory::createList(std::unique_ptr<Type> element_type) {
    return std::make_unique<ListType>(std::move(element_type));
}

std::unique_ptr<TupleType> TypeFactory::createTuple(std::vector<std::unique_ptr<Type>> elements) {
    return std::make_unique<TupleType>(std::move(elements));
}

std::unique_ptr<UnionType> TypeFactory::createUnion(std::vector<std::unique_ptr<Type>> types) {
    return std::make_unique<UnionType>(std::move(types));
}

std::unique_ptr<Type> TypeFactory::promoteNumericTypes(const Type* t1, const Type* t2) {
    if (!t1 || !t2) return createError("null type in promotion");
    
    // If either is float, result is float
    if (t1->isFloat() || t2->isFloat()) {
        if (t1->isNumeric() && t2->isNumeric()) {
            return createFloat();
        }
    }
    
    // If both are int, result is int
    if (t1->isInteger() && t2->isInteger()) {
        return createInt();
    }
    
    return createError("cannot promote non-numeric types");
}

std::unique_ptr<Type> TypeFactory::unifyTypes(const Type* t1, const Type* t2) {
    if (!t1 || !t2) return createError("null type in unification");
    
    if (t1->equals(t2)) {
        return std::unique_ptr<Type>(t1->clone());
    }
    
    // Handle numeric promotion
    if (t1->isNumeric() && t2->isNumeric()) {
        return promoteNumericTypes(t1, t2);
    }
    
    // Handle unknown types
    if (t1->isUnknown()) return std::unique_ptr<Type>(t2->clone());
    if (t2->isUnknown()) return std::unique_ptr<Type>(t1->clone());
    
    return createError("cannot unify incompatible types: " + 
                      t1->toString() + " and " + t2->toString());
}

std::unique_ptr<Type> TypeFactory::getCommonType(const std::vector<Type*>& types) {
    if (types.empty()) return createError("no types to unify");
    if (types.size() == 1) return std::unique_ptr<Type>(types[0]->clone());
    
    auto result = std::unique_ptr<Type>(types[0]->clone());
    for (size_t i = 1; i < types.size(); ++i) {
        result = unifyTypes(result.get(), types[i]);
        if (result->isError()) break;
    }
    
    return result;
}

// GenericInstantiator implementation
void GenericInstantiator::bindTypeVariable(const std::string& var_name, std::unique_ptr<Type> concrete_type) {
    type_bindings[var_name] = std::move(concrete_type);
}

Type* GenericInstantiator::getBinding(const std::string& var_name) const {
    auto it = type_bindings.find(var_name);
    return (it != type_bindings.end()) ? it->second.get() : nullptr;
}

void GenericInstantiator::clearBindings() {
    type_bindings.clear();
}

std::unique_ptr<Type> GenericInstantiator::instantiate(const Type* generic_type) {
    if (!generic_type) return nullptr;
    
    switch (generic_type->kind) {
        case TypeKind::GENERIC: {
            const GenericType* gen = static_cast<const GenericType*>(generic_type);
            Type* bound_type = getBinding(gen->parameter_name);
            if (bound_type) {
                return std::unique_ptr<Type>(bound_type->clone());
            }
            // Return the generic type if not bound
            return std::unique_ptr<Type>(generic_type->clone());
        }
        
        case TypeKind::FUNCTION: {
            const FunctionType* func = static_cast<const FunctionType*>(generic_type);
            return instantiateFunction(func);
        }
        
        case TypeKind::LIST: {
            const ListType* list = static_cast<const ListType*>(generic_type);
            auto elem_type = instantiate(list->element_type.get());
            return std::make_unique<ListType>(std::move(elem_type));
        }
        
        case TypeKind::TUPLE: {
            const TupleType* tuple = static_cast<const TupleType*>(generic_type);
            std::vector<std::unique_ptr<Type>> element_types;
            for (const auto& elem : tuple->element_types) {
                element_types.push_back(instantiate(elem.get()));
            }
            return std::make_unique<TupleType>(std::move(element_types));
        }
        
        case TypeKind::UNION: {
            const UnionType* union_type = static_cast<const UnionType*>(generic_type);
            std::vector<std::unique_ptr<Type>> types;
            for (const auto& type : union_type->types) {
                types.push_back(instantiate(type.get()));
            }
            return std::make_unique<UnionType>(std::move(types));
        }
        
        default:
            // For primitive types, return a clone
            return std::unique_ptr<Type>(generic_type->clone());
    }
}

std::unique_ptr<FunctionType> GenericInstantiator::instantiateFunction(const FunctionType* generic_func) {
    if (!generic_func) return nullptr;
    
    std::vector<std::unique_ptr<Type>> param_types;
    for (const auto& param : generic_func->param_types) {
        param_types.push_back(instantiate(param.get()));
    }
    
    auto return_type = instantiate(generic_func->return_type.get());
    
    return std::make_unique<FunctionType>(std::move(param_types), std::move(return_type));
}

// TypeConstraints implementation
void TypeConstraints::addConstraint(ConstraintKind kind, std::unique_ptr<Type> left, 
                                   std::unique_ptr<Type> right) {
    constraints.emplace_back(kind, std::move(left), std::move(right));
}

bool TypeConstraints::solve(GenericInstantiator& instantiator) {
    bool changed = true;
    while (changed) {
        changed = false;
        
        for (const auto& constraint : constraints) {
            switch (constraint.kind) {
                case ConstraintKind::EQUALS: {
                    // Handle T = concrete_type
                    if (constraint.left->kind == TypeKind::GENERIC && 
                        constraint.right->kind != TypeKind::GENERIC) {
                        
                        const GenericType* gen = static_cast<const GenericType*>(constraint.left.get());
                        if (!instantiator.getBinding(gen->parameter_name)) {
                            instantiator.bindTypeVariable(gen->parameter_name,
                                                        std::unique_ptr<Type>(constraint.right->clone()));
                            changed = true;
                        }
                    }
                    // Handle concrete_type = T
                    else if (constraint.right->kind == TypeKind::GENERIC && 
                            constraint.left->kind != TypeKind::GENERIC) {
                        
                        const GenericType* gen = static_cast<const GenericType*>(constraint.right.get());
                        if (!instantiator.getBinding(gen->parameter_name)) {
                            instantiator.bindTypeVariable(gen->parameter_name,
                                                        std::unique_ptr<Type>(constraint.left->clone()));
                            changed = true;
                        }
                    }
                    break;
                }
                
                case ConstraintKind::NUMERIC: {
                    if (constraint.left->kind == TypeKind::GENERIC) {
                        const GenericType* gen = static_cast<const GenericType*>(constraint.left.get());
                        if (!instantiator.getBinding(gen->parameter_name)) {
                            // Default numeric types to int
                            instantiator.bindTypeVariable(gen->parameter_name, TypeFactory::createInt());
                            changed = true;
                        }
                    }
                    break;
                }
                
                case ConstraintKind::COMPARABLE: {
                    if (constraint.left->kind == TypeKind::GENERIC) {
                        const GenericType* gen = static_cast<const GenericType*>(constraint.left.get());
                        if (!instantiator.getBinding(gen->parameter_name)) {
                            // Default comparable types to int
                            instantiator.bindTypeVariable(gen->parameter_name, TypeFactory::createInt());
                            changed = true;
                        }
                    }
                    break;
                }
                
                default:
                    // Other constraint kinds can be implemented later
                    break;
            }
        }
    }
    
    return true; // For now, assume all constraints can be solved
}

void TypeConstraints::clear() {
    constraints.clear();
}

// DiscriminatedUnionType implementation
DiscriminatedUnionType::DiscriminatedUnionType(std::vector<Variant> vars)
    : UnionType(std::vector<std::unique_ptr<Type>>{}), variants(std::move(vars)) {
    kind = TypeKind::DISCRIMINATED_UNION;
    name = "discriminated_union";
    
    // Add variant types to the union
    for (const auto& variant : variants) {
        types.push_back(std::unique_ptr<Type>(variant.data_type->clone()));
    }
}

Type* DiscriminatedUnionType::getVariantType(const std::string& tag) const {
    for (const auto& variant : variants) {
        if (variant.tag == tag) {
            return variant.data_type.get();
        }
    }
    return nullptr;
}

bool DiscriminatedUnionType::hasVariant(const std::string& tag) const {
    return getVariantType(tag) != nullptr;
}

std::string DiscriminatedUnionType::toString() const {
    std::stringstream ss;
    for (size_t i = 0; i < variants.size(); ++i) {
        if (i > 0) ss << " | ";
        ss << variants[i].tag << "(" << variants[i].data_type->toString() << ")";
    }
    return ss.str();
}

bool DiscriminatedUnionType::equals(const Type* other) const {
    if (!other || other->kind != TypeKind::DISCRIMINATED_UNION) return false;
    
    const DiscriminatedUnionType* other_union = 
        static_cast<const DiscriminatedUnionType*>(other);
    
    if (variants.size() != other_union->variants.size()) return false;
    
    // Check if all variants match
    for (const auto& variant : variants) {
        Type* other_variant_type = other_union->getVariantType(variant.tag);
        if (!other_variant_type || !variant.data_type->equals(other_variant_type)) {
            return false;
        }
    }
    
    return true;
}

Type* DiscriminatedUnionType::clone() const {
    std::vector<Variant> cloned_variants;
    for (const auto& variant : variants) {
        cloned_variants.emplace_back(variant.tag, 
                                   std::unique_ptr<Type>(variant.data_type->clone()));
    }
    return new DiscriminatedUnionType(std::move(cloned_variants));
}

// InterfaceType implementation
InterfaceType::InterfaceType(const std::string& name, std::vector<Method> meths)
    : Type(TypeKind::INTERFACE, name), interface_name(name), methods(std::move(meths)) {
}

std::string InterfaceType::toString() const {
    std::stringstream ss;
    ss << "interface " << interface_name << " { ";
    for (size_t i = 0; i < methods.size(); ++i) {
        if (i > 0) ss << "; ";
        ss << methods[i].name << ": " << methods[i].signature->toString();
    }
    ss << " }";
    return ss.str();
}

bool InterfaceType::equals(const Type* other) const {
    if (!other || other->kind != TypeKind::INTERFACE) return false;
    
    const InterfaceType* other_interface = static_cast<const InterfaceType*>(other);
    
    if (interface_name != other_interface->interface_name) return false;
    if (methods.size() != other_interface->methods.size()) return false;
    
    // Check if all methods match
    for (const auto& method : methods) {
        const FunctionType* other_method = other_interface->getMethod(method.name);
        if (!other_method || !method.signature->equals(other_method)) {
            return false;
        }
    }
    
    return true;
}

Type* InterfaceType::clone() const {
    std::vector<Method> cloned_methods;
    for (const auto& method : methods) {
        cloned_methods.emplace_back(method.name,
                                   std::unique_ptr<FunctionType>(
                                       static_cast<FunctionType*>(method.signature->clone())));
    }
    return new InterfaceType(interface_name, std::move(cloned_methods));
}

bool InterfaceType::isAssignableFrom(const Type* other) const {
    if (!other) return false;
    
    // For structural typing, check if other type has all required methods
    // This is simplified - in a real implementation, we'd need more sophisticated checks
    
    if (other->kind == TypeKind::INTERFACE) {
        const InterfaceType* other_interface = static_cast<const InterfaceType*>(other);
        
        // Check if other interface has all our methods
        for (const auto& method : methods) {
            if (!other_interface->hasMethod(method.name)) {
                return false;
            }
            
            const FunctionType* other_method = other_interface->getMethod(method.name);
            if (!method.signature->equals(other_method)) {
                return false;
            }
        }
        
        return true;
    }
    
    return false;
}

bool InterfaceType::hasMethod(const std::string& name) const {
    return getMethod(name) != nullptr;
}

const FunctionType* InterfaceType::getMethod(const std::string& name) const {
    for (const auto& method : methods) {
        if (method.name == name) {
            return method.signature.get();
        }
    }
    return nullptr;
}
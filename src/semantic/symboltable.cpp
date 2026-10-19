#include "symboltable.h"

namespace sem {

const char *typeName(Type type)
{
    switch (type) {
        case Type::Int:     return "int";
        case Type::Float:   return "float";
        case Type::Char:    return "char";
        case Type::String:  return "string";
        case Type::Boolean: return "boolean";
        case Type::Void:    return "void";
        case Type::Error:   break;
    }
    return "?";
}

const char *modalityName(Modality modality)
{
    switch (modality) {
        case Modality::Variable:        return "variavel";
        case Modality::Vector:          return "vetor";
        case Modality::Parameter:       return "parametro";
        case Modality::VectorParameter: return "parametro vetor";
        case Modality::Function:        return "funcao";
    }
    return "?";
}

bool isNumeric(Type type)
{
    return type == Type::Int || type == Type::Float;
}

bool isAssignable(Type target, Type value)
{
    if (target == Type::Error || value == Type::Error)
        return true;                       // already reported, stay quiet
    if (target == value)
        return true;
    return target == Type::Float && value == Type::Int;
}

void SymbolTable::reset()
{
    m_symbols.clear();
    m_scopes.clear();
    m_scopeNames.clear();
}

void SymbolTable::openScope(const std::string &name)
{
    m_scopes.emplace_back();
    m_scopeNames.push_back(name);
}

std::vector<int> SymbolTable::closeScope()
{
    if (m_scopes.empty())
        return {};

    std::vector<int> closed = m_scopes.back();
    m_scopes.pop_back();
    m_scopeNames.pop_back();
    return closed;
}

int SymbolTable::declare(Symbol symbol)
{
    if (m_scopes.empty())
        return kNoSymbol;
    if (lookupLocal(symbol.name) != kNoSymbol)
        return kNoSymbol;

    symbol.scope = m_scopeNames.back();
    symbol.level = currentLevel();

    const int index = static_cast<int>(m_symbols.size());
    m_symbols.push_back(std::move(symbol));
    m_scopes.back().push_back(index);
    return index;
}

int SymbolTable::lookup(const std::string &name) const
{
    for (auto scope = m_scopes.rbegin(); scope != m_scopes.rend(); ++scope) {
        for (int index : *scope) {
            if (m_symbols[index].name == name)
                return index;
        }
    }
    return kNoSymbol;
}

int SymbolTable::lookupLocal(const std::string &name) const
{
    if (m_scopes.empty())
        return kNoSymbol;

    for (int index : m_scopes.back()) {
        if (m_symbols[index].name == name)
            return index;
    }
    return kNoSymbol;
}

} // namespace sem

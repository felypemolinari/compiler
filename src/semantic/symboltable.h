#pragma once

// Symbol table for T3: identifiers carry a type, a modality and the scope they
// were declared in, and scopes nest (global -> subroutine -> block).
//
// Symbols are addressed by index rather than by pointer: every identifier the
// program declares stays in one vector so the IDE can list them all, including
// the ones whose scope has already been closed, and indices survive the
// reallocation that pointers would not.

#include <string>
#include <vector>

namespace sem {

const int kNoSymbol = -1;

enum class Type
{
    Int,
    Float,
    Char,
    String,
    Boolean,
    Void,      // a procedure: a subroutine declared with no return type
    Error      // already reported; poisons the expression so we stay quiet
};

enum class Modality
{
    Variable,
    Vector,
    Parameter,
    VectorParameter,
    Function
};

// What an expression produced. `isVector` marks a vector named without an
// index -- only ever legal as the argument of a vector parameter -- and `name`
// is kept so the diagnostics can say which identifier is at fault.
struct ValueType
{
    Type base = Type::Error;
    bool isVector = false;
    std::string name;

    ValueType() = default;
    ValueType(Type base, bool isVector = false, std::string name = {})
      : base(base), isVector(isVector), name(std::move(name)) { }
};

struct Symbol
{
    std::string name;
    Type type = Type::Error;
    Modality modality = Modality::Variable;
    std::string scope;
    int level = 0;
    int position = -1;          // byte offset of the declaration in the source
    int size = 0;               // vectors only; 0 when the size is unknown
    bool initialized = false;
    bool used = false;
    bool uninitializedReported = false;
    std::vector<ValueType> parameters;   // functions and procedures only

    bool isVectorLike() const
    {
        return modality == Modality::Vector || modality == Modality::VectorParameter;
    }

    bool isScalar() const
    {
        return modality == Modality::Variable || modality == Modality::Parameter;
    }
};

struct Diagnostic
{
    enum Severity { Error, Warning };

    Severity severity = Error;
    std::string message;
    int position = -1;
};

const char *typeName(Type type);
const char *modalityName(Modality modality);

// True when a value of type `value` may be stored in a `target` location.
// Only int -> float widens; everything else has to match exactly.
bool isAssignable(Type target, Type value);
bool isNumeric(Type type);

class SymbolTable
{
public:
    void reset();

    void openScope(const std::string &name);
    // Returns the symbols that lived in the scope being closed, so the caller
    // can warn about the ones nobody used.
    std::vector<int> closeScope();

    const std::string &currentScope() const { return m_scopeNames.back(); }
    int currentLevel() const { return static_cast<int>(m_scopes.size()) - 1; }

    // Inserts `symbol` in the innermost scope, filling in its scope and level.
    // Returns kNoSymbol when that scope already declares the name -- the
    // uniqueness rule -- leaving the table untouched.
    int declare(Symbol symbol);

    // Innermost scope first, so a local hides a global of the same name.
    // Scopes that are not open are never searched: that is what keeps an
    // identifier from leaking out of the subroutine it belongs to.
    int lookup(const std::string &name) const;
    int lookupLocal(const std::string &name) const;

    Symbol &at(int index) { return m_symbols[index]; }
    const Symbol &at(int index) const { return m_symbols[index]; }

    const std::vector<Symbol> &symbols() const { return m_symbols; }

private:
    std::vector<Symbol> m_symbols;
    std::vector<std::vector<int>> m_scopes;    // indices into m_symbols
    std::vector<std::string> m_scopeNames;
};

} // namespace sem

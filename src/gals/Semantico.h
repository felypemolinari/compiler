#ifndef SEMANTICO_H
#define SEMANTICO_H

// Semantic analyser (T3). GALS owns the name and the executeAction signature;
// everything else here is hand written, and regenerating the grammar must not
// overwrite this file or Semantico.cpp.
//
// Nothing is thrown: the actions collect errors and warnings so a single
// compilation reports every problem, and the "declared but never used" check
// only has an answer once a scope closes. Compiler::analyze reads diagnostics()
// and symbolTable() when the parse is over.

#include "Token.h"
#include "SemanticError.h"
#include "symboltable.h"

#include <string>
#include <vector>

class Semantico
{
public:
    Semantico() { reset(); }

    void executeAction(int action, const Token *token);

    void reset();

    const sem::SymbolTable &symbolTable() const { return m_table; }
    const std::vector<sem::Diagnostic> &diagnostics() const { return m_diagnostics; }
    bool hasErrors() const;

private:
    // An identifier kept aside until a later action needs it. Token objects
    // belong to the parser and are deleted right after the action runs, so the
    // lexeme and the position are copied out.
    struct Name
    {
        std::string lexeme;
        int position = -1;
        int symbol = sem::kNoSymbol;   // filled in once the name is resolved
    };

    // Open call, from the identifier up to the closing parenthesis. Calls nest
    // -- f(g(x)) -- so these are kept on a stack. `write` pushes one too, with
    // symbol == kNoSymbol, so that arguments are counted the same way.
    struct Call
    {
        int symbol = sem::kNoSymbol;
        std::string name;
        int position = -1;
        int arguments = 0;
        bool checkArguments = false;
    };

    // Left hand side of an assignment, pushed when the target is recognised
    // and popped when the value has been typed.
    struct Target
    {
        int symbol = sem::kNoSymbol;
        sem::Type type = sem::Type::Error;
        int position = -1;
    };

    void error(const std::string &message, int position);
    void warning(const std::string &message, int position);

    // -- expression stack --
    void push(const sem::ValueType &value);
    sem::ValueType pop();

    // -- helpers shared by several actions --
    static sem::Type typeOfToken(int tokenId);
    static const char *operatorName(int tokenId);
    int pendingOperator();

    // Looks `name` up and reports it when it is not visible from here.
    int resolve(const Name &name);
    void markUsed(int symbol, int position);
    void checkIndex(const sem::ValueType &index, int position);
    void checkAssignment(const Target &target, const sem::ValueType &value);
    void reportUnused(const std::vector<int> &symbols);

    void binaryArithmetic(int position);
    void binaryIntegerOnly(int position, bool modulo);
    void binaryRelational(int position);
    void binaryLogical(int position);

    sem::SymbolTable m_table;
    std::vector<sem::Diagnostic> m_diagnostics;

    sem::Type m_declaredType = sem::Type::Error;   // the last <tipo> that was read
    int m_pendingSize = 0;                         // the last <tamanho> that was read

    std::vector<Name> m_names;          // identifiers waiting for a later action
    std::vector<sem::ValueType> m_stack;
    std::vector<int> m_operators;       // token ids of operators being applied
    std::vector<Call> m_calls;
    std::vector<Target> m_targets;

    // The body of a subroutine must not open a scope of its own: the
    // parameters are already in the subroutine's scope and a local that
    // repeats a parameter name has to collide with it.
    std::vector<int> m_subroutines;     // symbol index of each open subroutine
    std::vector<bool> m_blockOwnsScope;
    bool m_bodyScopeIsOpen = false;
    int m_blockCount = 0;
};

#endif

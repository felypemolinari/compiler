#include "Semantico.h"
#include "Constants.h"

// Semantic actions declared in grammar/compiler.gals are dispatched here.
// The grammar has no actions yet, so nothing runs; throw a SemanticError from
// this method to report a semantic problem -- compiler.cpp already turns it
// into a message with line and column in the IDE.
void Semantico::executeAction(int action, const Token *token)
{
    (void) action;
    (void) token;
}

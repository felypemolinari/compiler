#include "Semantico.h"
#include "Constants.h"

#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Semantic actions for T3, dispatched from grammar/compiler.gals.
//
//  1 program header        12 close block scope     25 OR            38 vector element
//  2 end of program        13 assign: scalar        26 AND           39 end call in expr
//  3 type of a declaration 14 assign: value         27 NOT           40 start call in expr
//  4 declare variable      15 assign: vector        28 relational    41 one argument
//  5 vector being declared 16 assign: index         29 bitwise       42 start write
//  6 declare vector        17 assign: value         30 + - * /       43 end write
//  7 size of the vector    18 start call stmt       31 %             44 literal
//  8 declare function      19 end call stmt         32 unary + -     45 condition
//  9 declare procedure     20 read variable         33 unary ~       46 declare parameter
// 10 end of subroutine     21 read vector           34 keep operator 47 declare vector param
// 11 open block scope      22 read index            35 use variable
//                          23 return value          36 vector used
//                          24 return empty          37 index of a vector
//
// Each action runs with the token shifted just before it, which is why an
// action that needs an identifier sits immediately after that id in the
// grammar. Nothing is thrown: diagnostics are collected so one compilation
// reports every problem it found.
// ---------------------------------------------------------------------------

using sem::Modality;
using sem::Symbol;
using sem::Type;
using sem::ValueType;
using sem::kNoSymbol;

namespace {

std::string quoted(const std::string &name)
{
    return "'" + name + "'";
}

// Vector sizes are written in decimal, hexadecimal or binary.
int sizeOfLiteral(const std::string &lexeme)
{
    try {
        if (lexeme.size() > 2 && lexeme[0] == '0') {
            if (lexeme[1] == 'x' || lexeme[1] == 'X')
                return std::stoi(lexeme.substr(2), nullptr, 16);
            if (lexeme[1] == 'b' || lexeme[1] == 'B')
                return std::stoi(lexeme.substr(2), nullptr, 2);
        }
        return std::stoi(lexeme);
    } catch (const std::exception &) {
        return 0;    // out of range; the size is only shown in the table
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Bookkeeping
// ---------------------------------------------------------------------------

void Semantico::reset()
{
    m_table.reset();
    m_diagnostics.clear();

    m_declaredType = Type::Error;
    m_pendingSize = 0;

    m_names.clear();
    m_stack.clear();
    m_operators.clear();
    m_calls.clear();
    m_targets.clear();
    m_subroutines.clear();
    m_blockOwnsScope.clear();
    m_bodyScopeIsOpen = false;
    m_blockCount = 0;

    // Opened here rather than in action 1 so that a declaration always has a
    // scope to land in, even if the program header failed to parse.
    m_table.openScope("global");
}

bool Semantico::hasErrors() const
{
    for (const sem::Diagnostic &diagnostic : m_diagnostics) {
        if (diagnostic.severity == sem::Diagnostic::Error)
            return true;
    }
    return false;
}

void Semantico::error(const std::string &message, int position)
{
    m_diagnostics.push_back({sem::Diagnostic::Error, message, position});
}

void Semantico::warning(const std::string &message, int position)
{
    m_diagnostics.push_back({sem::Diagnostic::Warning, message, position});
}

void Semantico::push(const ValueType &value)
{
    m_stack.push_back(value);
}

ValueType Semantico::pop()
{
    if (m_stack.empty())
        return ValueType(Type::Error);

    ValueType value = m_stack.back();
    m_stack.pop_back();
    return value;
}

int Semantico::pendingOperator()
{
    if (m_operators.empty())
        return 0;

    const int id = m_operators.back();
    m_operators.pop_back();
    return id;
}

Type Semantico::typeOfToken(int tokenId)
{
    switch (tokenId) {
        case t_type_int:     return Type::Int;
        case t_type_float:   return Type::Float;
        case t_type_string:  return Type::String;
        case t_type_boolean: return Type::Boolean;
        case t_type_char:    return Type::Char;
        default:             return Type::Error;
    }
}

const char *Semantico::operatorName(int tokenId)
{
    switch (tokenId) {
        case t_op_add:         return "+";
        case t_op_sub:         return "-";
        case t_op_mul:         return "*";
        case t_op_div:         return "/";
        case t_op_mod:         return "%";
        case t_op_and:         return "AND";
        case t_op_or:          return "OR";
        case t_op_not:         return "NOT";
        case t_op_equal:       return "==";
        case t_op_not_equal:   return "!=";
        case t_op_less:        return "<";
        case t_op_greater:     return ">";
        case t_op_less_eq:     return "<=";
        case t_op_greater_eq:  return ">=";
        case t_op_shift_left:  return "<<";
        case t_op_shift_right: return ">>";
        case t_op_bit_and:     return "&";
        case t_op_bit_or:      return "|";
        case t_op_bit_xor:     return "^";
        case t_op_bit_not:     return "~";
        default:               return "?";
    }
}

// ---------------------------------------------------------------------------
// Shared checks
// ---------------------------------------------------------------------------

int Semantico::resolve(const Name &name)
{
    const int symbol = m_table.lookup(name.lexeme);
    if (symbol == kNoSymbol) {
        error(quoted(name.lexeme) + " nao foi declarado no escopo "
                  + quoted(m_table.currentScope()),
              name.position);
    }
    return symbol;
}

void Semantico::markUsed(int symbol, int position)
{
    if (symbol == kNoSymbol)
        return;

    Symbol &entry = m_table.at(symbol);
    entry.used = true;

    // Only a warning: the assignment asks us not to reject the program.
    if (!entry.initialized && !entry.uninitializedReported) {
        entry.uninitializedReported = true;
        warning(quoted(entry.name) + " pode estar sendo usado sem ter sido "
                "inicializado (lixo de memoria)",
                position);
    }
}

void Semantico::checkIndex(const ValueType &index, int position)
{
    if (index.base == Type::Error)
        return;

    if (index.isVector) {
        error("o vetor " + quoted(index.name) + " precisa de um indice", position);
    } else if (index.base != Type::Int) {
        error(std::string("o indice de um vetor precisa ser int, e nao ")
                  + quoted(sem::typeName(index.base)),
              position);
    }
}

void Semantico::checkAssignment(const Target &target, const ValueType &value)
{
    if (target.type == Type::Error || value.base == Type::Error)
        return;

    if (value.isVector) {
        error("o vetor " + quoted(value.name)
                  + " precisa de um indice para ser usado em uma atribuicao",
              target.position);
        return;
    }

    if (!sem::isAssignable(target.type, value.base)) {
        error(std::string("nao e possivel atribuir um valor ")
                  + quoted(sem::typeName(value.base)) + " a um destino "
                  + quoted(sem::typeName(target.type)),
              target.position);
    }
}

void Semantico::reportUnused(const std::vector<int> &symbols)
{
    for (int index : symbols) {
        const Symbol &entry = m_table.at(index);
        if (entry.used)
            continue;

        warning(quoted(entry.name) + " (" + sem::modalityName(entry.modality)
                    + ") foi declarado no escopo " + quoted(entry.scope)
                    + " e nunca foi usado",
                entry.position);
    }
}

// ---------------------------------------------------------------------------
// Operators
// ---------------------------------------------------------------------------

void Semantico::binaryArithmetic(int position)
{
    const ValueType right = pop();
    const ValueType left = pop();
    const int op = pendingOperator();

    if (left.base == Type::Error || right.base == Type::Error) {
        push(ValueType(Type::Error));
        return;
    }

    if (left.isVector || right.isVector) {
        const ValueType &vector = left.isVector ? left : right;
        error("o vetor " + quoted(vector.name) + " precisa de um indice para "
              "ser usado com o operador " + quoted(operatorName(op)),
              position);
        push(ValueType(Type::Error));
        return;
    }

    if (!sem::isNumeric(left.base) || !sem::isNumeric(right.base)) {
        error(std::string("o operador ") + quoted(operatorName(op))
                  + " nao se aplica a " + quoted(sem::typeName(left.base))
                  + " e " + quoted(sem::typeName(right.base)),
              position);
        push(ValueType(Type::Error));
        return;
    }

    const bool real = left.base == Type::Float || right.base == Type::Float;
    push(ValueType(real ? Type::Float : Type::Int));
}

void Semantico::binaryIntegerOnly(int position, bool modulo)
{
    const ValueType right = pop();
    const ValueType left = pop();
    const int op = pendingOperator();

    if (left.base == Type::Error || right.base == Type::Error) {
        push(ValueType(Type::Error));
        return;
    }

    if (left.isVector || right.isVector) {
        const ValueType &vector = left.isVector ? left : right;
        error("o vetor " + quoted(vector.name) + " precisa de um indice para "
              "ser usado com o operador " + quoted(operatorName(op)),
              position);
        push(ValueType(Type::Error));
        return;
    }

    if (left.base != Type::Int || right.base != Type::Int) {
        error(std::string("o operador ") + quoted(operatorName(op))
                  + (modulo ? " exige dois valores int, e recebeu "
                            : " so vale para int, e recebeu ")
                  + quoted(sem::typeName(left.base)) + " e "
                  + quoted(sem::typeName(right.base)),
              position);
        push(ValueType(Type::Error));
        return;
    }

    push(ValueType(Type::Int));
}

void Semantico::binaryRelational(int position)
{
    const ValueType right = pop();
    const ValueType left = pop();
    const int op = pendingOperator();

    if (left.base == Type::Error || right.base == Type::Error) {
        push(ValueType(Type::Boolean));
        return;
    }

    if (left.isVector || right.isVector) {
        const ValueType &vector = left.isVector ? left : right;
        error("o vetor " + quoted(vector.name) + " precisa de um indice para "
              "ser comparado",
              position);
        push(ValueType(Type::Boolean));
        return;
    }

    // == and != accept anything comparable to itself; the ordering operators
    // additionally refuse boolean, which has no order.
    const bool equality = op == t_op_equal || op == t_op_not_equal;
    const bool bothNumeric = sem::isNumeric(left.base) && sem::isNumeric(right.base);
    const bool sameType = left.base == right.base;
    const bool ordered = sameType && left.base != Type::Boolean;

    if (!bothNumeric && !(equality ? sameType : ordered)) {
        error(std::string("o operador ") + quoted(operatorName(op))
                  + " nao compara " + quoted(sem::typeName(left.base))
                  + " com " + quoted(sem::typeName(right.base)),
              position);
    }

    push(ValueType(Type::Boolean));
}

void Semantico::binaryLogical(int position)
{
    const ValueType right = pop();
    const ValueType left = pop();
    const int op = pendingOperator();

    if (left.base == Type::Error || right.base == Type::Error) {
        push(ValueType(Type::Boolean));
        return;
    }

    if (left.isVector || right.isVector || left.base != Type::Boolean
        || right.base != Type::Boolean) {
        error(std::string("o operador ") + quoted(operatorName(op))
                  + " exige dois valores boolean, e recebeu "
                  + quoted(sem::typeName(left.base)) + " e "
                  + quoted(sem::typeName(right.base)),
              position);
    }

    push(ValueType(Type::Boolean));
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

void Semantico::executeAction(int action, const Token *token)
{
    const std::string lexeme = token != nullptr ? token->getLexeme() : std::string();
    const int position = token != nullptr ? token->getPosition() : -1;
    const int tokenId = token != nullptr ? token->getId() : 0;

    switch (action) {

    // -- program ------------------------------------------------------------

    case 1:     // program <id> .
        break;  // the global scope is already open; nothing to record

    case 2: {   // ... end .
        reportUnused(m_table.closeScope());
        break;
    }

    // -- declarations -------------------------------------------------------

    case 3:     // <tipo>
        m_declaredType = typeOfToken(tokenId);
        break;

    case 4: {   // <nome_decl> ::= id
        Symbol entry;
        entry.name = lexeme;
        entry.type = m_declaredType;
        entry.modality = Modality::Variable;
        entry.position = position;
        if (m_table.declare(entry) == kNoSymbol) {
            error(quoted(lexeme) + " ja foi declarado no escopo "
                      + quoted(m_table.currentScope()),
                  position);
        }
        break;
    }

    case 5:     // <nome_decl> ::= id . [ <tamanho> ]
        m_names.push_back({lexeme, position, kNoSymbol});
        m_pendingSize = 0;
        break;

    case 6: {   // ... id [ <tamanho> ] .
        const Name name = m_names.empty() ? Name{} : m_names.back();
        if (!m_names.empty())
            m_names.pop_back();

        Symbol entry;
        entry.name = name.lexeme;
        entry.type = m_declaredType;
        entry.modality = Modality::Vector;
        entry.position = name.position;
        entry.size = m_pendingSize;
        if (m_table.declare(entry) == kNoSymbol) {
            error(quoted(name.lexeme) + " ja foi declarado no escopo "
                      + quoted(m_table.currentScope()),
                  name.position);
        }
        m_pendingSize = 0;
        break;
    }

    case 7:     // <tamanho>
        m_pendingSize = sizeOfLiteral(lexeme);
        break;

    // -- subroutines --------------------------------------------------------

    case 8:     // function <tipo> id
    case 9: {   // function id            (a procedure: no return type)
        Symbol entry;
        entry.name = lexeme;
        entry.type = action == 8 ? m_declaredType : Type::Void;
        entry.modality = Modality::Function;
        entry.position = position;
        entry.initialized = true;

        const int symbol = m_table.declare(entry);
        if (symbol == kNoSymbol) {
            error(quoted(lexeme) + " ja foi declarado no escopo "
                      + quoted(m_table.currentScope()),
                  position);
        }

        // The scope opens either way, so the body is still analysed.
        m_subroutines.push_back(symbol);
        m_table.openScope(lexeme);
        m_bodyScopeIsOpen = true;
        break;
    }

    case 46:    // <param> ::= <tipo> id
    case 47: {  // <param> ::= <tipo> id [ ]
        const bool vector = action == 47;

        Symbol entry;
        entry.name = lexeme;
        entry.type = m_declaredType;
        entry.modality = vector ? Modality::VectorParameter : Modality::Parameter;
        entry.position = position;
        entry.initialized = true;   // the caller supplies the value

        if (m_table.declare(entry) == kNoSymbol) {
            error(quoted(lexeme) + " ja foi declarado no escopo "
                      + quoted(m_table.currentScope()),
                  position);
        }

        if (!m_subroutines.empty() && m_subroutines.back() != kNoSymbol) {
            m_table.at(m_subroutines.back())
                .parameters.push_back(ValueType(m_declaredType, vector, lexeme));
        }
        break;
    }

    case 10:    // end of a subroutine
        reportUnused(m_table.closeScope());
        if (!m_subroutines.empty())
            m_subroutines.pop_back();
        break;

    // -- blocks -------------------------------------------------------------

    case 11: {  // {
        // The body of a subroutine reuses the scope its parameters are in, so
        // a local that repeats a parameter name collides with it.
        const bool ownsScope = !m_bodyScopeIsOpen;
        if (ownsScope) {
            m_table.openScope(m_table.currentScope() + "/bloco"
                              + std::to_string(++m_blockCount));
        } else {
            m_bodyScopeIsOpen = false;
        }
        m_blockOwnsScope.push_back(ownsScope);
        break;
    }

    case 12: {  // }
        const bool ownsScope = !m_blockOwnsScope.empty() && m_blockOwnsScope.back();
        if (!m_blockOwnsScope.empty())
            m_blockOwnsScope.pop_back();
        if (ownsScope)
            reportUnused(m_table.closeScope());
        break;
    }

    // -- assignments --------------------------------------------------------

    case 13: {  // id = ...
        Target target;
        target.position = position;
        target.symbol = resolve({lexeme, position, kNoSymbol});

        if (target.symbol != kNoSymbol) {
            const Symbol &entry = m_table.at(target.symbol);
            if (entry.modality == Modality::Function) {
                error(quoted(lexeme) + " e uma sub-rotina e nao pode receber "
                      "uma atribuicao", position);
            } else if (entry.isVectorLike()) {
                error("o vetor " + quoted(lexeme) + " precisa de um indice",
                      position);
            } else {
                target.type = entry.type;
            }
        }
        m_targets.push_back(target);
        break;
    }

    case 15: {  // id [ ... ] = ...
        Target target;
        target.position = position;
        target.symbol = resolve({lexeme, position, kNoSymbol});

        if (target.symbol != kNoSymbol) {
            const Symbol &entry = m_table.at(target.symbol);
            if (!entry.isVectorLike()) {
                error(quoted(lexeme) + " nao e um vetor", position);
            } else {
                target.type = entry.type;
            }
        }
        m_targets.push_back(target);
        break;
    }

    case 16:    // the index of an indexed assignment
        checkIndex(pop(), position);
        break;

    case 14:    // the value of an assignment
    case 17: {
        const ValueType value = pop();
        const Target target = m_targets.empty() ? Target{} : m_targets.back();
        if (!m_targets.empty())
            m_targets.pop_back();

        checkAssignment(target, value);
        if (target.symbol != kNoSymbol)
            m_table.at(target.symbol).initialized = true;
        break;
    }

    // -- calls --------------------------------------------------------------

    case 18:    // id ( ... ) .        as a command
    case 40: {  // id ( ... )          inside an expression
        Call call;
        call.name = lexeme;
        call.position = position;
        call.symbol = resolve({lexeme, position, kNoSymbol});

        if (call.symbol != kNoSymbol) {
            Symbol &entry = m_table.at(call.symbol);
            if (entry.modality != Modality::Function) {
                error(quoted(lexeme) + " nao e uma sub-rotina", position);
                call.symbol = kNoSymbol;
            } else {
                entry.used = true;
                call.checkArguments = true;
            }
        }
        m_calls.push_back(call);
        break;
    }

    case 41: {  // one argument of the call that is open
        const ValueType value = pop();
        if (m_calls.empty())
            break;

        Call &call = m_calls.back();
        const int index = call.arguments++;
        if (!call.checkArguments || value.base == Type::Error)
            break;

        const Symbol &function = m_table.at(call.symbol);
        if (index >= static_cast<int>(function.parameters.size()))
            break;    // the count is reported once, when the call closes

        const ValueType &parameter = function.parameters[index];
        if (parameter.isVector != value.isVector) {
            error("o parametro " + std::to_string(index + 1) + " de "
                      + quoted(call.name) + " espera "
                      + (parameter.isVector ? "um vetor" : "um valor simples")
                      + ", e recebeu "
                      + (value.isVector ? "um vetor" : "um valor simples"),
                  call.position);
        } else if (!sem::isAssignable(parameter.base, value.base)) {
            error("o parametro " + std::to_string(index + 1) + " de "
                      + quoted(call.name) + " espera "
                      + quoted(sem::typeName(parameter.base)) + ", e recebeu "
                      + quoted(sem::typeName(value.base)),
                  call.position);
        }
        break;
    }

    case 19:    // the call closes
    case 39: {
        const Call call = m_calls.empty() ? Call{} : m_calls.back();
        if (!m_calls.empty())
            m_calls.pop_back();

        Type result = Type::Error;

        if (call.checkArguments) {
            const Symbol &function = m_table.at(call.symbol);
            const int expected = static_cast<int>(function.parameters.size());
            if (call.arguments != expected) {
                error("a sub-rotina " + quoted(call.name) + " espera "
                          + std::to_string(expected) + " parametro(s), e recebeu "
                          + std::to_string(call.arguments),
                      call.position);
            }
            result = function.type;
        }

        if (action == 39) {
            // Used as a value, so a procedure has nothing to offer.
            if (result == Type::Void) {
                error(quoted(call.name) + " e um procedimento e nao devolve "
                      "um valor", call.position);
                result = Type::Error;
            }
            push(ValueType(result));
        }
        break;
    }

    case 42: {  // write ( ... ) -- arguments are counted but not checked
        Call call;
        call.name = lexeme;
        call.position = position;
        m_calls.push_back(call);
        break;
    }

    case 43:
        if (!m_calls.empty())
            m_calls.pop_back();
        break;

    // -- read ---------------------------------------------------------------

    case 20: {  // read ( id )
        const int symbol = resolve({lexeme, position, kNoSymbol});
        if (symbol == kNoSymbol)
            break;

        Symbol &entry = m_table.at(symbol);
        if (entry.modality == Modality::Function)
            error(quoted(lexeme) + " e uma sub-rotina e nao pode ser lida", position);
        else if (entry.isVectorLike())
            error("o vetor " + quoted(lexeme) + " precisa de um indice", position);
        else
            entry.initialized = true;
        break;
    }

    case 21: {  // read ( id [ ... ] )
        Name name{lexeme, position, kNoSymbol};
        name.symbol = resolve(name);

        if (name.symbol != kNoSymbol) {
            Symbol &entry = m_table.at(name.symbol);
            if (!entry.isVectorLike()) {
                error(quoted(lexeme) + " nao e um vetor", position);
                name.symbol = kNoSymbol;
            } else {
                entry.initialized = true;
            }
        }
        m_names.push_back(name);
        break;
    }

    case 22:    // the index of a vector being read
        checkIndex(pop(), position);
        if (!m_names.empty())
            m_names.pop_back();
        break;

    // -- return -------------------------------------------------------------

    case 23: {  // return <expressao> .
        const ValueType value = pop();
        if (m_subroutines.empty()) {
            error("'return' com valor so pode aparecer dentro de uma sub-rotina",
                  position);
            break;
        }

        const int symbol = m_subroutines.back();
        if (symbol == kNoSymbol || value.base == Type::Error)
            break;

        const Symbol &function = m_table.at(symbol);
        if (function.type == Type::Void) {
            error("o procedimento " + quoted(function.name)
                      + " nao pode devolver um valor",
                  position);
        } else if (value.isVector) {
            error("o vetor " + quoted(value.name) + " precisa de um indice "
                  "para ser devolvido", position);
        } else if (!sem::isAssignable(function.type, value.base)) {
            error("a funcao " + quoted(function.name) + " devolve "
                      + quoted(sem::typeName(function.type)) + ", e o return traz "
                      + quoted(sem::typeName(value.base)),
                  position);
        }
        break;
    }

    case 24: {  // return .
        if (m_subroutines.empty() || m_subroutines.back() == kNoSymbol)
            break;

        const Symbol &function = m_table.at(m_subroutines.back());
        if (function.type != Type::Void) {
            error("a funcao " + quoted(function.name) + " precisa devolver um valor "
                      + quoted(sem::typeName(function.type)),
                  position);
        }
        break;
    }

    // -- operators ----------------------------------------------------------

    case 34:    // keeps the operator so the action that folds it can name it
        m_operators.push_back(tokenId);
        break;

    case 25:    // OR
    case 26:    // AND
        binaryLogical(position);
        break;

    case 27: {  // NOT
        const ValueType value = pop();
        const int op = pendingOperator();
        if (value.base != Type::Error
            && (value.isVector || value.base != Type::Boolean)) {
            error(std::string("o operador ") + quoted(operatorName(op))
                      + " exige um valor boolean, e recebeu "
                      + quoted(sem::typeName(value.base)),
                  position);
        }
        push(ValueType(Type::Boolean));
        break;
    }

    case 28:    // == != < > <= >=
        binaryRelational(position);
        break;

    case 29:    // | ^ & << >>
        binaryIntegerOnly(position, false);
        break;

    case 30:    // + - * /
        binaryArithmetic(position);
        break;

    case 31:    // %
        binaryIntegerOnly(position, true);
        break;

    case 32: {  // unary + and -
        const ValueType value = pop();
        const int op = pendingOperator();
        if (value.base == Type::Error) {
            push(ValueType(Type::Error));
            break;
        }
        if (value.isVector || !sem::isNumeric(value.base)) {
            error(std::string("o operador unario ") + quoted(operatorName(op))
                      + " nao se aplica a " + quoted(sem::typeName(value.base)),
                  position);
            push(ValueType(Type::Error));
            break;
        }
        push(ValueType(value.base));
        break;
    }

    case 33: {  // unary ~
        const ValueType value = pop();
        const int op = pendingOperator();
        if (value.base == Type::Error) {
            push(ValueType(Type::Error));
            break;
        }
        if (value.isVector || value.base != Type::Int) {
            error(std::string("o operador ") + quoted(operatorName(op))
                      + " so vale para int, e recebeu "
                      + quoted(sem::typeName(value.base)),
                  position);
            push(ValueType(Type::Error));
            break;
        }
        push(ValueType(Type::Int));
        break;
    }

    // -- operands -----------------------------------------------------------

    case 35: {  // an identifier used as a value
        const int symbol = resolve({lexeme, position, kNoSymbol});
        if (symbol == kNoSymbol) {
            push(ValueType(Type::Error));
            break;
        }

        const Symbol &entry = m_table.at(symbol);
        if (entry.modality == Modality::Function) {
            error("a sub-rotina " + quoted(lexeme)
                      + " precisa ser chamada com parenteses",
                  position);
            push(ValueType(Type::Error));
            break;
        }

        markUsed(symbol, position);
        // A vector named without an index only makes sense as the argument of
        // a vector parameter; every other action rejects it.
        push(ValueType(entry.type, entry.isVectorLike(), entry.name));
        break;
    }

    case 36: {  // a vector being indexed
        Name name{lexeme, position, kNoSymbol};
        name.symbol = resolve(name);

        if (name.symbol != kNoSymbol) {
            if (!m_table.at(name.symbol).isVectorLike()) {
                error(quoted(lexeme) + " nao e um vetor", position);
                name.symbol = kNoSymbol;
            } else {
                markUsed(name.symbol, position);
            }
        }
        m_names.push_back(name);
        break;
    }

    case 37:    // the index itself
        checkIndex(pop(), position);
        break;

    case 38: {  // the element the index selects
        const Name name = m_names.empty() ? Name{} : m_names.back();
        if (!m_names.empty())
            m_names.pop_back();

        push(name.symbol == kNoSymbol
                 ? ValueType(Type::Error)
                 : ValueType(m_table.at(name.symbol).type));
        break;
    }

    case 44:    // a literal
        switch (tokenId) {
            case t_lit_int_dec:
            case t_lit_int_hex:
            case t_lit_int_bin: push(ValueType(Type::Int));     break;
            case t_lit_real:    push(ValueType(Type::Float));   break;
            case t_lit_char:    push(ValueType(Type::Char));    break;
            case t_lit_string:  push(ValueType(Type::String));  break;
            case t_true:
            case t_false:       push(ValueType(Type::Boolean)); break;
            default:            push(ValueType(Type::Error));   break;
        }
        break;

    // -- conditions ---------------------------------------------------------

    case 45: {  // if / while / for / do..while
        const ValueType value = pop();
        if (value.base == Type::Error)
            break;

        if (value.isVector) {
            error("o vetor " + quoted(value.name) + " precisa de um indice",
                  position);
        } else if (value.base != Type::Boolean) {
            error(std::string("a condicao precisa ser boolean, e nao ")
                      + quoted(sem::typeName(value.base)),
                  position);
        }
        break;
    }

    default:
        break;
    }
}

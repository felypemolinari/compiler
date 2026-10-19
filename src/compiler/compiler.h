#pragma once

#include <QString>
#include <QVector>

// One row of the symbol table, already turned into text for the IDE panel.
struct SymbolRow
{
    QString name;
    QString type;
    QString modality;
    QString scope;
    QString size;          // vectors only; empty otherwise
    QString initialized;
    QString used;
    int line = -1;
};

// A semantic problem. Errors fail the compilation; warnings do not.
struct Message
{
    bool error = true;
    QString text;
    int line = -1;
    int column = -1;
};

struct CompileResult
{
    bool success = true;
    QString message;
    int line = -1;     // 1 = first line; -1 when not applicable
    int column = -1;

    // Filled in whenever the parse completed, even if the program has
    // semantic errors -- the table is useful either way.
    QVector<SymbolRow> symbols;
    QVector<Message> messages;
};

namespace Compiler {

// Entry point of the compilation: runs the GALS analyser over `source`.
CompileResult analyze(const QString &source);

} // namespace Compiler

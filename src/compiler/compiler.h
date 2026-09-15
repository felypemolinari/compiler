#pragma once

#include <QString>

struct CompileResult
{
    bool success = true;
    QString message;
    int line = -1;     // 1 = first line; -1 when not applicable
    int column = -1;
};

namespace Compiler {

// Entry point of the compilation: runs the GALS analyser over `source`.
CompileResult analyze(const QString &source);

} // namespace Compiler

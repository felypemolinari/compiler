// Headless front end: compiles the files given on the command line and prints
// the diagnostics and the symbol table. The IDE is the deliverable, but this
// is what makes tests/run_tests.sh possible.
//
//     ./build/compiler-cli [--table] tests/00_completo.txt

#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include "compiler.h"

namespace {

void printSymbolTable(QTextStream &out, const CompileResult &result)
{
    out << "  symbol table (" << result.symbols.size() << "):\n";
    out << QString("    %1 %2 %3 %4 %5 %6 %7\n")
               .arg("identificador", -16).arg("tipo", -8).arg("modalidade", -16)
               .arg("escopo", -18).arg("tam", 4).arg("ini", 4).arg("uso", 4);

    for (const SymbolRow &symbol : result.symbols) {
        out << QString("    %1 %2 %3 %4 %5 %6 %7\n")
                   .arg(symbol.name, -16)
                   .arg(symbol.type, -8)
                   .arg(symbol.modality, -16)
                   .arg(symbol.scope, -18)
                   .arg(symbol.size.isEmpty() ? QString("-") : symbol.size, 4)
                   .arg(symbol.initialized, 4)
                   .arg(symbol.used, 4);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QTextStream out(stdout);
    QStringList paths;
    bool showTable = false;

    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == "--table")
            showTable = true;
        else
            paths << argument;
    }

    if (paths.isEmpty()) {
        out << "usage: compiler-cli [--table] <file>...\n";
        return 2;
    }

    int failures = 0;

    for (const QString &path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            out << path << ": could not open\n";
            ++failures;
            continue;
        }

        QTextStream in(&file);
        const CompileResult result = Compiler::analyze(in.readAll());

        out << path << ": " << (result.success ? "OK" : "FAIL") << "\n";
        out << "  " << QString(result.message).replace('\n', "\n  ") << "\n";

        for (const Message &message : result.messages) {
            out << QString("  line %1, column %2: %3: %4\n")
                       .arg(message.line, 4)
                       .arg(message.column)
                       .arg(message.error ? "erro " : "aviso")
                       .arg(message.text);
        }

        if (showTable)
            printSymbolTable(out, result);

        out << "\n";
        failures += result.success ? 0 : 1;
    }

    return failures == 0 ? 0 : 1;
}

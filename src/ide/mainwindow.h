#pragma once

#include <QMainWindow>
#include <QPalette>

struct CompileResult;

class CodeEditor;
class QAction;
class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QTableWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newFile();
    void openFile();
    bool save();
    bool saveAs();
    void compile();
    void showTeam();
    void toggleDarkMode(bool enabled);
    void updateCursorPosition();

private:
    void buildUi();
    void buildSymbolDock();
    void buildActions();
    void showSymbolTable(const CompileResult &result);
    bool maybeSave();
    bool writeToFile(const QString &path);
    void setCurrentFile(const QString &path);
    void log(const QString &text);
    void applyTheme(bool dark);

    CodeEditor *m_editor = nullptr;
    QPlainTextEdit *m_messages = nullptr;
    QTableWidget *m_symbols = nullptr;
    QDockWidget *m_symbolDock = nullptr;
    QLabel *m_position = nullptr;
    QAction *m_darkModeAction = nullptr;
    QPalette m_lightPalette;
    QString m_currentFile;
};

#pragma once

#include <QMainWindow>
#include <QPalette>

class CodeEditor;
class QAction;
class QLabel;
class QPlainTextEdit;

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
    void buildActions();
    bool maybeSave();
    bool writeToFile(const QString &path);
    void setCurrentFile(const QString &path);
    void log(const QString &text);
    void applyTheme(bool dark);

    CodeEditor *m_editor = nullptr;
    QPlainTextEdit *m_messages = nullptr;
    QLabel *m_position = nullptr;
    QAction *m_darkModeAction = nullptr;
    QPalette m_lightPalette;
    QString m_currentFile;
};

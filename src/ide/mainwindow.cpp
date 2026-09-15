#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTextStream>
#include <QToolBar>

#include "codeeditor.h"
#include "compiler.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_lightPalette = QApplication::palette();

    buildUi();
    buildActions();
    setCurrentFile(QString());

    // Restore the theme chosen the last time the IDE was used.
    const bool dark = QSettings().value("appearance/darkMode", false).toBool();
    m_darkModeAction->setChecked(dark);
    applyTheme(dark);

    resize(1000, 700);
    log("IDE ready. Press F7 to compile.");
}

void MainWindow::buildUi()
{
    m_editor = new CodeEditor(this);

    m_messages = new QPlainTextEdit(this);
    m_messages->setReadOnly(true);

    // ---- Assignment requirement: messages in font size 14 ----
    QFont messagesFont("Monospace");
    messagesFont.setStyleHint(QFont::Monospace);
    messagesFont.setPointSize(14);
    m_messages->setFont(messagesFont);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_editor);
    splitter->addWidget(m_messages);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({520, 180});

    setCentralWidget(splitter);

    m_position = new QLabel(this);
    statusBar()->addPermanentWidget(m_position);

    connect(m_editor, &QPlainTextEdit::cursorPositionChanged,
            this, &MainWindow::updateCursorPosition);
    connect(m_editor->document(), &QTextDocument::modificationChanged,
            this, &QWidget::setWindowModified);

    updateCursorPosition();
}

void MainWindow::buildActions()
{
    auto *fileMenu = menuBar()->addMenu("&File");
    auto *viewMenu = menuBar()->addMenu("&View");
    auto *compilerMenu = menuBar()->addMenu("&Compiler");
    auto *helpMenu = menuBar()->addMenu("&Help");
    auto *toolBar = addToolBar("Main");
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto addItem = [&](QMenu *menu, const QString &text,
                       const QKeySequence &shortcut, auto slot) {
        auto *action = new QAction(text, this);
        action->setShortcut(shortcut);
        connect(action, &QAction::triggered, this, slot);
        menu->addAction(action);
        toolBar->addAction(action);
        return action;
    };

    addItem(fileMenu, "&New", QKeySequence::New, &MainWindow::newFile);
    addItem(fileMenu, "&Open...", QKeySequence::Open, &MainWindow::openFile);
    addItem(fileMenu, "&Save", QKeySequence::Save, &MainWindow::save);
    addItem(fileMenu, "Save &As...", QKeySequence::SaveAs, &MainWindow::saveAs);
    fileMenu->addSeparator();
    auto *quit = new QAction("E&xit", this);
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quit);

    toolBar->addSeparator();
    m_darkModeAction = new QAction("&Dark Mode", this);
    m_darkModeAction->setCheckable(true);
    m_darkModeAction->setShortcut(QKeySequence("Ctrl+D"));
    m_darkModeAction->setToolTip("Switch between the light and dark themes");
    connect(m_darkModeAction, &QAction::toggled, this, &MainWindow::toggleDarkMode);
    viewMenu->addAction(m_darkModeAction);
    toolBar->addAction(m_darkModeAction);

    toolBar->addSeparator();
    addItem(compilerMenu, "&Compile", QKeySequence(Qt::Key_F7), &MainWindow::compile);
    addItem(helpMenu, "&Team", QKeySequence(Qt::Key_F1), &MainWindow::showTeam);
}

void MainWindow::newFile()
{
    if (!maybeSave())
        return;

    m_editor->clear();
    m_editor->clearErrorLine();
    m_messages->clear();
    setCurrentFile(QString());
    log("New program.");
}

void MainWindow::openFile()
{
    if (!maybeSave())
        return;

    const QString path = QFileDialog::getOpenFileName(
        this, "Open program", QString(), "Text files (*.txt);;All files (*)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Open",
                             QString("Could not open %1:\n%2")
                                 .arg(path, file.errorString()));
        return;
    }

    QTextStream stream(&file);
    m_editor->setPlainText(stream.readAll());
    m_editor->clearErrorLine();
    setCurrentFile(path);
    log(QString("File opened: %1").arg(path));
}

bool MainWindow::save()
{
    if (m_currentFile.isEmpty())
        return saveAs();
    return writeToFile(m_currentFile);
}

bool MainWindow::saveAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Save program", QString(), "Text files (*.txt);;All files (*)");
    if (path.isEmpty())
        return false;
    return writeToFile(path);
}

bool MainWindow::writeToFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save",
                             QString("Could not save %1:\n%2")
                                 .arg(path, file.errorString()));
        return false;
    }

    QTextStream stream(&file);
    stream << m_editor->toPlainText();

    setCurrentFile(path);
    log(QString("File saved: %1").arg(path));
    return true;
}

void MainWindow::compile()
{
    m_editor->clearErrorLine();
    m_messages->clear();

    const CompileResult result = Compiler::analyze(m_editor->toPlainText());

    if (result.success) {
        log(result.message);
    } else {
        QString text = result.message;
        if (result.line > 0) {
            text = QString("Line %1, column %2\n%3")
                       .arg(result.line)
                       .arg(result.column)
                       .arg(result.message);
            m_editor->markErrorLine(result.line);
        }
        log(text);
    }
}

void MainWindow::showTeam()
{
    QMessageBox::information(this, "Team",
                             "Compiler IDE\n\n"
                             "Course: Compilers\n"
                             "Team: Felype Molinari and Vinicius Andriani");
}

void MainWindow::toggleDarkMode(bool enabled)
{
    applyTheme(enabled);
    QSettings().setValue("appearance/darkMode", enabled);
    log(enabled ? "Dark mode enabled." : "Light mode enabled.");
}

void MainWindow::applyTheme(bool dark)
{
    QPalette palette = m_lightPalette;

    if (dark) {
        const QColor window(45, 45, 48);
        const QColor base(30, 30, 30);
        const QColor text(220, 220, 220);
        const QColor disabled(130, 130, 130);

        palette.setColor(QPalette::Window, window);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::AlternateBase, QColor(52, 52, 56));
        palette.setColor(QPalette::ToolTipBase, window);
        palette.setColor(QPalette::ToolTipText, text);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::PlaceholderText, disabled);
        palette.setColor(QPalette::Button, window);
        palette.setColor(QPalette::ButtonText, text);
        palette.setColor(QPalette::BrightText, QColor(255, 90, 90));
        palette.setColor(QPalette::Link, QColor(88, 166, 255));
        palette.setColor(QPalette::Highlight, QColor(38, 100, 160));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Mid, QColor(140, 140, 140));

        palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    }

    QApplication::setPalette(palette);
    m_editor->setDarkMode(dark);
}

void MainWindow::updateCursorPosition()
{
    const QTextCursor cursor = m_editor->textCursor();
    m_position->setText(QString("Line %1, Column %2")
                            .arg(cursor.blockNumber() + 1)
                            .arg(cursor.positionInBlock() + 1));
}

void MainWindow::log(const QString &text)
{
    m_messages->appendPlainText(text);
}

void MainWindow::setCurrentFile(const QString &path)
{
    m_currentFile = path;
    m_editor->document()->setModified(false);
    setWindowModified(false);

    const QString name = path.isEmpty() ? "untitled.txt"
                                        : QFileInfo(path).fileName();
    setWindowTitle(QString("%1[*] - Compiler IDE").arg(name));
    statusBar()->showMessage(path.isEmpty() ? "New file" : path);
}

bool MainWindow::maybeSave()
{
    if (!m_editor->document()->isModified())
        return true;

    const auto answer = QMessageBox::warning(
        this, "Compiler IDE",
        "The program has been modified.\nDo you want to save your changes?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

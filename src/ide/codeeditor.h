#pragma once

#include <QPlainTextEdit>

class LineNumberArea;

// Source editor: 14pt monospaced font, line numbers on the left margin
// and highlighting of the line where the compiler reported an error.
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void paintLineNumberArea(QPaintEvent *event);
    int lineNumberAreaWidth() const;

    void markErrorLine(int line);   // line is 1-based
    void clearErrorLine();

    void setDarkMode(bool dark);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateViewportMargin(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void updateExtraSelections();

private:
    LineNumberArea *m_lineNumberArea = nullptr;
    int m_errorLine = -1;
    bool m_darkMode = false;
};

#include "codeeditor.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QTextBlock>
#include <QTextEdit>

// Side strip that draws the line numbers.
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor)
        : QWidget(editor), m_editor(editor) {}

    QSize sizeHint() const override
    {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        m_editor->paintLineNumberArea(event);
    }

private:
    CodeEditor *m_editor;
};

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    // ---- Assignment requirement: font size 14 ----
    QFont font("Monospace");
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    font.setPointSize(14);
    setFont(font);

    setTabStopDistance(4 * QFontMetricsF(font).horizontalAdvance(' '));
    setLineWrapMode(QPlainTextEdit::NoWrap);

    m_lineNumberArea = new LineNumberArea(this);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateViewportMargin);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::updateExtraSelections);

    updateViewportMargin(0);
    updateExtraSelections();
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CodeEditor::updateViewportMargin(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy != 0)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateViewportMargin(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(),
                                        lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::paintLineNumberArea(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), palette().color(QPalette::Window));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const bool isErrorLine = (blockNumber + 1 == m_errorLine);
            const QColor errorPen = m_darkMode ? QColor(255, 120, 120)
                                               : QColor(180, 30, 30);
            painter.setPen(isErrorLine ? errorPen
                                       : palette().color(QPalette::Mid));
            painter.drawText(0, top,
                             m_lineNumberArea->width() - 6, fontMetrics().height(),
                             Qt::AlignRight, QString::number(blockNumber + 1));
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::updateExtraSelections()
{
    QList<QTextEdit::ExtraSelection> selections;

    // Line the cursor is currently on
    QTextEdit::ExtraSelection currentLine;
    currentLine.format.setBackground(palette().color(QPalette::AlternateBase));
    currentLine.format.setProperty(QTextFormat::FullWidthSelection, true);
    currentLine.cursor = textCursor();
    currentLine.cursor.clearSelection();
    selections.append(currentLine);

    // Line reported by the parser
    if (m_errorLine > 0) {
        QTextBlock block = document()->findBlockByNumber(m_errorLine - 1);
        if (block.isValid()) {
            QTextEdit::ExtraSelection error;
            error.format.setBackground(m_darkMode ? QColor(96, 42, 42)
                                                  : QColor(255, 205, 205));
            error.format.setProperty(QTextFormat::FullWidthSelection, true);
            error.cursor = QTextCursor(block);
            selections.append(error);
        }
    }

    setExtraSelections(selections);
}

void CodeEditor::markErrorLine(int line)
{
    m_errorLine = line;

    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (block.isValid()) {
        QTextCursor cursor(block);
        setTextCursor(cursor);
        centerCursor();
    }

    updateExtraSelections();
    m_lineNumberArea->update();
    setFocus();
}

void CodeEditor::setDarkMode(bool dark)
{
    m_darkMode = dark;
    updateExtraSelections();
    m_lineNumberArea->update();
}

void CodeEditor::clearErrorLine()
{
    m_errorLine = -1;
    updateExtraSelections();
    m_lineNumberArea->update();
}

#include "TerminalWidget.hpp"
#include "ui_TerminalWidget.h"

#include <QApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>

// TerminalStreambuf implementation
TerminalStreambuf::TerminalStreambuf(TerminalWidget * terminal, bool isErrorStream)
    : m_terminal(terminal),
      m_originalBuffer(nullptr),
      m_isErrorStream(isErrorStream) {
}

TerminalStreambuf::~TerminalStreambuf() {
    if (!m_buffer.empty()) {
        flushBuffer();
    }
}

std::streambuf::int_type TerminalStreambuf::overflow(int_type ch) {
    if (ch != EOF) {
        m_buffer += static_cast<char>(ch);

        // Forward to original buffer if available (for Qt Creator output)
        if (m_originalBuffer) {
            m_originalBuffer->sputc(ch);
        }

        // If we hit a newline, flush the buffer
        if (ch == '\n') {
            flushBuffer();
        }
    }
    return ch;
}

std::streamsize TerminalStreambuf::xsputn(char const * s, std::streamsize count) {
    m_buffer.append(s, count);

    // Forward to original buffer if available (for Qt Creator output)
    if (m_originalBuffer) {
        m_originalBuffer->sputn(s, count);
    }

    // Check for newlines in the buffer
    size_t pos = m_buffer.find('\n');
    while (pos != std::string::npos) {
        flushBuffer();
        pos = m_buffer.find('\n');
    }

    return count;
}

void TerminalStreambuf::flushBuffer() {
    if (!m_buffer.empty() && m_terminal) {
        QString text = QString::fromStdString(m_buffer);

        // Use Qt's signal-slot mechanism to safely update UI from any thread
        QMetaObject::invokeMethod(m_terminal, "onOutputReceived", Qt::QueuedConnection,
                                  Q_ARG(QString, text), Q_ARG(bool, m_isErrorStream));

        m_buffer.clear();
    }
}

// TerminalWidget implementation
TerminalWidget::TerminalWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TerminalWidget),
      m_textEdit(nullptr),
      m_clearButton(nullptr),
      m_originalCoutBuffer(nullptr),
      m_originalCerrBuffer(nullptr),
      m_currentLineCount(0) {

    ui->setupUi(this);
    setupUI();

    // Connect the signal to slot for thread-safe UI updates
    connect(this, &TerminalWidget::outputReceived, this, &TerminalWidget::onOutputReceived);
}

TerminalWidget::~TerminalWidget() {
    restoreStreamRedirection();
    delete ui;
}

void TerminalWidget::setupUI() {
    // Create main layout
    QVBoxLayout * mainLayout = new QVBoxLayout(this);

    // Create text edit for terminal output
    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setFont(QFont("Consolas", 10));// Monospace font

    // Set terminal-like appearance
    m_textEdit->setStyleSheet(
            "QTextEdit {"
            "  background-color: #000000;"// Black background
            "  color: #FFFFFF;"           // White text
            "  border: 1px solid #333333;"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 10pt;"
            "}"
            "QScrollBar:vertical {"
            "  background-color: #2b2b2b;"
            "  width: 12px;"
            "  border: none;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background-color: #555555;"
            "  border-radius: 6px;"
            "  min-height: 20px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "  background-color: #777777;"
            "}");

    // Create clear button
    m_clearButton = new QPushButton("Clear", this);
    m_clearButton->setMaximumWidth(80);
    m_clearButton->setStyleSheet(
            "QPushButton {"
            "  background-color: #333333;"
            "  color: #FFFFFF;"
            "  border: 1px solid #555555;"
            "  padding: 4px 8px;"
            "  border-radius: 3px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #444444;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #222222;"
            "}");

    // Create button layout
    QHBoxLayout * buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_clearButton);

    // Add widgets to main layout
    mainLayout->addWidget(m_textEdit);
    mainLayout->addLayout(buttonLayout);

    // Connect clear button
    connect(m_clearButton, &QPushButton::clicked, this, &TerminalWidget::clearTerminal);

    setLayout(mainLayout);
}

void TerminalWidget::openWidget() {
    // Setup stream redirection when widget is opened
    setupStreamRedirection();

    // Add welcome message
    QString welcomeMsg = QString("[%1] Terminal output capture started\n")
                                 .arg(getCurrentTimestamp());
    appendOutput(welcomeMsg, false);
}

void TerminalWidget::setupStreamRedirection() {
    if (m_coutBuffer || m_cerrBuffer) {
        return;// Already set up
    }

    // Store original buffers
    m_originalCoutBuffer = std::cout.rdbuf();
    m_originalCerrBuffer = std::cerr.rdbuf();

    // Create our custom buffers
    m_coutBuffer = std::make_unique<TerminalStreambuf>(this, false);
    m_cerrBuffer = std::make_unique<TerminalStreambuf>(this, true);

    // Set original buffers in our custom buffers (for tee functionality)
    m_coutBuffer->setOriginalBuffer(m_originalCoutBuffer);
    m_cerrBuffer->setOriginalBuffer(m_originalCerrBuffer);

    // Redirect streams to our custom buffers
    std::cout.rdbuf(m_coutBuffer.get());
    std::cerr.rdbuf(m_cerrBuffer.get());
}

void TerminalWidget::restoreStreamRedirection() {
    if (m_originalCoutBuffer) {
        std::cout.rdbuf(m_originalCoutBuffer);
        m_originalCoutBuffer = nullptr;
    }

    if (m_originalCerrBuffer) {
        std::cerr.rdbuf(m_originalCerrBuffer);
        m_originalCerrBuffer = nullptr;
    }

    m_coutBuffer.reset();
    m_cerrBuffer.reset();
}

void TerminalWidget::appendOutput(QString const & text, bool isError) {
    if (!m_textEdit) return;

    // Get current cursor and move to end
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    // Create format for the text
    QTextCharFormat format;
    if (isError) {
        format.setForeground(QColor("#FF6B6B"));// Red for errors
    } else {
        format.setForeground(QColor("#FFFFFF"));// White for normal output
    }

    // Insert the text with formatting
    cursor.insertText(text, format);

    // Count lines and enforce buffer size
    m_currentLineCount += text.count('\n');
    enforceMaxBufferSize();

    // Auto-scroll to bottom
    QScrollBar * scrollBar = m_textEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void TerminalWidget::onOutputReceived(QString const & text, bool isError) {
    // Add timestamp to each line
    QStringList lines = text.split('\n');
    QString timestampedText;

    for (int i = 0; i < lines.size(); ++i) {
        if (i < lines.size() - 1 || !lines[i].isEmpty()) {
            if (!lines[i].isEmpty()) {
                timestampedText += QString("[%1] %2\n")
                                           .arg(getCurrentTimestamp())
                                           .arg(lines[i]);
            } else {
                timestampedText += "\n";
            }
        }
    }

    if (!timestampedText.isEmpty()) {
        appendOutput(timestampedText, isError);
    }
}

void TerminalWidget::clearTerminal() {
    if (m_textEdit) {
        m_textEdit->clear();
        m_currentLineCount = 0;

        // Add clear message
        QString clearMsg = QString("[%1] Terminal cleared\n")
                                   .arg(getCurrentTimestamp());
        appendOutput(clearMsg, false);
    }
}

void TerminalWidget::enforceMaxBufferSize() {
    if (m_currentLineCount > MAX_BUFFER_SIZE) {
        // Remove approximately 20% of the oldest content
        QTextCursor cursor = m_textEdit->textCursor();
        cursor.movePosition(QTextCursor::Start);

        // Select first 20% of lines
        int linesToRemove = MAX_BUFFER_SIZE / 5;
        for (int i = 0; i < linesToRemove; ++i) {
            cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
        }

        // Remove selected text
        cursor.removeSelectedText();

        m_currentLineCount -= linesToRemove;

        // Add buffer trimmed message
        cursor.movePosition(QTextCursor::Start);
        QTextCharFormat format;
        format.setForeground(QColor("#FFAA00"));// Orange for system messages
        cursor.insertText(QString("[%1] [SYSTEM] Buffer trimmed - oldest %2 lines removed\n")
                                  .arg(getCurrentTimestamp())
                                  .arg(linesToRemove),
                          format);
    }
}

QString TerminalWidget::getCurrentTimestamp() const {
    return QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
}

//#include "TerminalWidget.moc"
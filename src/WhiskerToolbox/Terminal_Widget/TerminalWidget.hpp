#ifndef TERMINALWIDGET_HPP
#define TERMINALWIDGET_HPP

#include <QWidget>

#include <iostream>
#include <memory>
#include <sstream>
#include <streambuf>

class QHBoxLayout;
class QPushButton;
class QTextEdit;

QT_BEGIN_NAMESPACE
namespace Ui {
class TerminalWidget;
}
QT_END_NAMESPACE

class TerminalWidget;

// Custom streambuf to capture cout/cerr output
class TerminalStreambuf : public std::streambuf {
public:
    explicit TerminalStreambuf(TerminalWidget * terminal, bool isErrorStream = false);
    ~TerminalStreambuf() override;

    void setOriginalBuffer(std::streambuf * original) { m_originalBuffer = original; }

protected:
    int_type overflow(int_type ch) override;
    std::streamsize xsputn(char const * s, std::streamsize count) override;

private:
    TerminalWidget * m_terminal;
    std::streambuf * m_originalBuffer;
    bool m_isErrorStream;
    std::string m_buffer;

    void flushBuffer();
};

class TerminalWidget : public QWidget {
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget * parent = nullptr);
    ~TerminalWidget() override;

    void openWidget();
    void appendOutput(QString const & text, bool isError = false);

private slots:
    void clearTerminal();
    void onOutputReceived(QString const & text, bool isError);

private:
    Ui::TerminalWidget * ui;

    QTextEdit * m_textEdit;
    QPushButton * m_clearButton;

    std::unique_ptr<TerminalStreambuf> m_coutBuffer;
    std::unique_ptr<TerminalStreambuf> m_cerrBuffer;
    std::streambuf * m_originalCoutBuffer;
    std::streambuf * m_originalCerrBuffer;

    static constexpr int MAX_BUFFER_SIZE = 10000;// Maximum number of lines
    int m_currentLineCount;

    void setupUI();
    void setupStreamRedirection();
    void restoreStreamRedirection();
    void enforceMaxBufferSize();
    QString getCurrentTimestamp() const;

signals:
    void outputReceived(QString const & text, bool isError);
};

#endif// TERMINALWIDGET_HPP
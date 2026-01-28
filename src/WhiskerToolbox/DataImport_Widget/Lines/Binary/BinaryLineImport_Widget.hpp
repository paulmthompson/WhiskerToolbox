#ifndef BINARY_LINE_IMPORT_WIDGET_HPP
#define BINARY_LINE_IMPORT_WIDGET_HPP

/**
 * @file BinaryLineImport_Widget.hpp
 * @brief Widget for configuring binary line data import options
 * 
 * This widget provides UI controls for loading line data from binary files
 * (Cap'n Proto format). Requires ENABLE_CAPNPROTO to be set at build time.
 */

#include <QWidget>
#include <QString>

namespace Ui {
class BinaryLineImport_Widget;
}

/**
 * @brief Widget for binary line import configuration
 * 
 * Allows users to load line data from Cap'n Proto binary files.
 */
class BinaryLineImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit BinaryLineImport_Widget(QWidget * parent = nullptr);
    ~BinaryLineImport_Widget() override;

signals:
    /**
     * @brief Emitted when user requests to load a binary line file
     * @param filepath Path to the binary file
     */
    void loadBinaryFileRequested(QString filepath);

private slots:
    void _onLoadBinaryFileButtonPressed();

private:
    Ui::BinaryLineImport_Widget * ui;
};

#endif // BINARY_LINE_IMPORT_WIDGET_HPP

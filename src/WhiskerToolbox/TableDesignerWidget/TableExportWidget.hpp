#ifndef TABLEEXPORTWIDGET_HPP
#define TABLEEXPORTWIDGET_HPP

#include <QString>
#include <QWidget>

namespace Ui {
class TableExportWidget;
}

/**
 * @brief Widget to configure and trigger CSV export for a TableView.
 */
class TableExportWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a new TableExportWidget.
     * @param parent Parent widget.
     */
    explicit TableExportWidget(QWidget * parent = nullptr);
    ~TableExportWidget() override;

    /**
     * @brief Get selected delimiter label.
     * @return Delimiter label (Comma, Space, Tab).
     */
    QString getDelimiterText() const;

    /**
     * @brief Get selected line ending label.
     * @return Line ending label (e.g., "LF (\n)", "CRLF (\r\n)").
     */
    QString getLineEndingText() const;

    /**
     * @brief Get decimal precision.
     * @return Integer precision.
     */
    int getPrecision() const;

    /**
     * @brief Whether to include a header row.
     * @return True if header row should be included.
     */
    bool isHeaderIncluded() const;

signals:
    /**
     * @brief Emitted when the user clicks the Export button.
     */
    void exportClicked();

private:
    Ui::TableExportWidget * ui;
};

#endif// TABLEEXPORTWIDGET_HPP

#ifndef TABLEJSONWIDGET_HPP
#define TABLEJSONWIDGET_HPP

#include <QWidget>
#include <QString>

namespace Ui { class TableJSONWidget; }

/**
 * @brief Widget to view/edit a JSON template for table creation.
 *
 * Provides:
 * - A text area for JSON content
 * - A button to load JSON from a file
 * - A button to apply the JSON to update the table UI
 */
class TableJSONWidget : public QWidget {
    Q_OBJECT

public:
    explicit TableJSONWidget(QWidget * parent = nullptr);
    ~TableJSONWidget() override;

    /**
     * @brief Set JSON text in the editor.
     */
    void setJsonText(QString const & text);

    /**
     * @brief Get JSON text from the editor.
     */
    QString getJsonText() const;

    /**
     * @brief For tests: Force the next Load JSON action to use this path.
     */
    void setForcedLoadPathForTests(QString const & path);

signals:
    /**
     * @brief Emitted when user clicks Update Table. Carries current JSON text.
     */
    void updateRequested(QString const & jsonText);

private slots:
    void onLoadJsonClicked();
    void onUpdateTableClicked();
    void onSaveJsonClicked();

private:
    Ui::TableJSONWidget * ui;
    QString m_forcedLoadPath;
};

#endif // TABLEJSONWIDGET_HPP



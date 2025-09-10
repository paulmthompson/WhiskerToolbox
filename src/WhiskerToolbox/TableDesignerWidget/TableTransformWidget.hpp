#ifndef TABLETRANSFORMWIDGET_HPP
#define TABLETRANSFORMWIDGET_HPP

#include <QWidget>
#include <QString>
#include <vector>
#include <string>

namespace Ui { class TableTransformWidget; }

/**
 * @brief Widget for configuring and applying transforms to tables
 *
 * This widget provides controls for:
 * - Selecting transform type (currently PCA)
 * - Configuring transform options (center, standardize)
 * - Specifying include/exclude columns
 * - Setting output name
 * - Applying the transform
 */
class TableTransformWidget : public QWidget {
    Q_OBJECT

public:
    explicit TableTransformWidget(QWidget * parent = nullptr);
    ~TableTransformWidget() override;

    /**
     * @brief Get the selected transform type
     * @return Transform type string (e.g., "PCA")
     */
    QString getTransformType() const;

    /**
     * @brief Get whether centering is enabled
     * @return True if center checkbox is checked
     */
    bool isCenterEnabled() const;

    /**
     * @brief Get whether standardization is enabled
     * @return True if standardize checkbox is checked
     */
    bool isStandardizeEnabled() const;

    /**
     * @brief Get the list of columns to include
     * @return Vector of column names to include (empty if all)
     */
    std::vector<std::string> getIncludeColumns() const;

    /**
     * @brief Get the list of columns to exclude
     * @return Vector of column names to exclude
     */
    std::vector<std::string> getExcludeColumns() const;

    /**
     * @brief Get the output name for the transformed table
     * @return Output name string
     */
    QString getOutputName() const;

    /**
     * @brief Set the output name for the transformed table
     * @param name The output name to set
     */
    void setOutputName(const QString& name);

signals:
    /**
     * @brief Emitted when the Apply Transform button is clicked
     */
    void applyTransformClicked();

private slots:
    /**
     * @brief Handle Apply Transform button click
     */
    void onApplyTransformClicked();

private:
    Ui::TableTransformWidget * ui;

    /**
     * @brief Parse a comma-separated list of strings
     * @param text The comma-separated text to parse
     * @return Vector of trimmed strings
     */
    std::vector<std::string> parseCommaSeparatedList(const QString& text) const;
};

#endif // TABLETRANSFORMWIDGET_HPP

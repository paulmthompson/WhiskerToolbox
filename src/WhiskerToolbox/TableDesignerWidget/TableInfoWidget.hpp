#ifndef TABLEINFOWIDGET_HPP
#define TABLEINFOWIDGET_HPP

#include <QWidget>
#include <QString>

namespace Ui { class TableInfoWidget; }

/**
 * @brief Small widget to edit table name/description and trigger save.
 */
class TableInfoWidget : public QWidget {
    Q_OBJECT

public:
    explicit TableInfoWidget(QWidget * parent = nullptr);
    ~TableInfoWidget() override;

    QString getName() const;
    QString getDescription() const;
    void setName(QString const & name);
    void setDescription(QString const & desc);

signals:
    void saveClicked();

private:
    Ui::TableInfoWidget * ui;
};

#endif // TABLEINFOWIDGET_HPP



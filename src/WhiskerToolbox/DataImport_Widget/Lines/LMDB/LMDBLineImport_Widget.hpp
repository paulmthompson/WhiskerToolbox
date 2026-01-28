#ifndef LMDB_LINE_IMPORT_WIDGET_HPP
#define LMDB_LINE_IMPORT_WIDGET_HPP

/**
 * @file LMDBLineImport_Widget.hpp
 * @brief Widget for configuring LMDB line data import options
 * 
 * This widget is a placeholder for future LMDB line data loading support.
 * LMDB (Lightning Memory-Mapped Database) support is not yet implemented.
 */

#include <QWidget>
#include <QString>

namespace Ui {
class LMDBLineImport_Widget;
}

/**
 * @brief Widget for LMDB line import configuration (placeholder)
 * 
 * This is a placeholder widget for future LMDB support.
 */
class LMDBLineImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit LMDBLineImport_Widget(QWidget * parent = nullptr);
    ~LMDBLineImport_Widget() override;

signals:

private:
    Ui::LMDBLineImport_Widget * ui;
};

#endif // LMDB_LINE_IMPORT_WIDGET_HPP

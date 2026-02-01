#include "CSVPointImport_Widget.hpp"
#include "ui_CSVPointImport_Widget.h"

#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"

#include <QSpinBox>

CSVPointImport_Widget::CSVPointImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVPointImport_Widget) {
    ui->setupUi(this);
    
    connect(ui->load_single_csv_button, &QPushButton::clicked, this, [this]() {
        CSVPointLoaderOptions options;
        options.frame_column = ui->frame_column_spinbox->value();
        options.x_column = ui->x_column_spinbox->value();
        options.y_column = ui->y_column_spinbox->value();

        QString delimiterText = ui->delimiter_combo->currentText();
        if (delimiterText == "Space") {
            options.column_delim = " ";
        } else if (delimiterText == "Comma") {
            options.column_delim = ",";
        } else {
            options.column_delim = ","; // Default to comma
        }
        // filepath will be set by the parent PointImport_Widget

        emit loadSingleCSVFileRequested(options);
    });
}

CSVPointImport_Widget::~CSVPointImport_Widget() {
    delete ui;
}

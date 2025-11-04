#include "CSVPointLoader_Widget.hpp"
#include "ui_CSVPointLoader_Widget.h"

#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"

#include <QSpinBox>

CSVPointLoader_Widget::CSVPointLoader_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVPointLoader_Widget) {
    ui->setupUi(this);
    connect(ui->load_single_csv_button, &QPushButton::clicked, this, [this]() {
        CSVPointLoaderOptions options;
        options.frame_column = ui->frame_column_spinbox->value();
        options.x_column = ui->x_column_spinbox->value();
        options.y_column = ui->y_column_spinbox->value();

        QString delimiterText = ui->delimiter_combo->currentText();
        if (delimiterText == "Space") {
            options.column_delim = ' ';
        } else if (delimiterText == "Comma") {
            options.column_delim = ',';
        } else {
            // Should not happen with current UI, but good to be defensive
            options.column_delim = ',';// Default to comma
        }
        // filename will be set by the parent Point_Loader_Widget

        emit loadSingleCSVFileRequested(options);
    });
}

CSVPointLoader_Widget::~CSVPointLoader_Widget() {
    delete ui;
}
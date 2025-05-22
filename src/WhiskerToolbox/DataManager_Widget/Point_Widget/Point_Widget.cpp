#include "Point_Widget.hpp"
#include "ui_Point_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "PointTableModel.hpp"
#include "IO_Widgets/PointLoaderWidget/CSV/CSVPointSaver_Widget.hpp"

#include <QFileDialog>
#include <QPushButton>
#include <QTableView>
#include <QComboBox>
#include <QStackedWidget>

#include <fstream>
#include <iostream>

Point_Widget::Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Point_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _point_table_model = new PointTableModel(this);
    ui->tableView->setModel(_point_table_model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Point_Widget::_onExportTypeChanged);

    connect(ui->csv_point_saver_widget, &CSVPointSaver_Widget::saveCSVRequested,
            this, &Point_Widget::_handleSaveCSVRequested);

    connect(ui->tableView, &QTableView::doubleClicked, this, &Point_Widget::_handleTableViewDoubleClicked);
    connect(ui->movePointsButton, &QPushButton::clicked, this, &Point_Widget::_movePointsButton_clicked);
    connect(ui->deletePointsButton, &QPushButton::clicked, this, &Point_Widget::_deletePointsButton_clicked);

    ui->stacked_saver_options->setCurrentWidget(ui->csv_point_saver_widget);
}

Point_Widget::~Point_Widget() {
    delete ui;
}

void Point_Widget::openWidget() {
    this->show();
}

void Point_Widget::setActiveKey(std::string const & key) {
    if (_active_key == key && _callback_id != -1) {
        updateTable();
        _populateMoveToPointDataComboBox();
        return;
    }
    removeCallbacks();

    _active_key = key;
    updateTable();
    _populateMoveToPointDataComboBox();

    if (!_active_key.empty()) {
        auto point_data = _data_manager->getData<PointData>(_active_key);
        if (point_data) {
            _callback_id = _data_manager->addCallbackToData(_active_key, [this]() { _onDataChanged(); });
        } else {
            std::cerr << "Point_Widget: No PointData found for key '" << _active_key << "' to attach callback." << std::endl;
        }
    }
}

void Point_Widget::updateTable() {
    if (!_active_key.empty()) {
        auto point_data = _data_manager->getData<PointData>(_active_key);
        _point_table_model->setPoints(point_data.get());
    } else {
        _point_table_model->setPoints(nullptr);
    }
}

void Point_Widget::_onExportTypeChanged(int index) {
    if (ui->export_type_combo->itemText(index) == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_point_saver_widget);
    }
}

void Point_Widget::_handleSaveCSVRequested(QString filename) {
    if (filename.isEmpty()) {
        std::cout << "Point_Widget: Save filename is empty, using default or last provided." << std::endl;
    }
    _saveToCSVFile(filename);
}

void Point_Widget::_saveToCSVFile(QString const& filename) {
    if (_active_key.empty()) {
        std::cerr << "Point_Widget: No active key selected, cannot save CSV." << std::endl;
        return;
    }

    std::fstream fout;
    std::filesystem::path output_path = _data_manager->getOutputPath();
    output_path /= filename.toStdString();

    fout.open(output_path, std::fstream::out);
    if (!fout.is_open()) {
        std::cerr << "Point_Widget: Failed to open file for saving: " << output_path.string() << std::endl;
        return;
    }

    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (!point_data) {
        std::cerr << "Point_Widget: Cannot save CSV, PointData for key '" << _active_key << "' not found." << std::endl;
        fout.close();
        return;
    }

    for (auto const& timePointsPair : point_data->GetAllPointsAsRange()) {
        fout << timePointsPair.time;
        for (size_t i = 0; i < timePointsPair.points.size(); ++i) {
            fout << "," << timePointsPair.points[i].x << "," << timePointsPair.points[i].y;
        }
        fout << "\n";
    }
    fout.close();
    std::cout << "Point_Widget: Successfully saved points to " << output_path.string() << std::endl;
}

void Point_Widget::loadFrame(int frame_id) {
    if (ui->propagate_checkbox->isChecked()) {
        _propagateLabel(frame_id);
    }
    _previous_frame = frame_id;
}

void Point_Widget::_propagateLabel(int frame_id) {
    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (!point_data) return;

    auto prev_points = point_data->getPointsAtTime(_previous_frame);

    for (int i = _previous_frame + 1; i <= frame_id; i++) {
        point_data->overwritePointsAtTime(i, prev_points);
    }
}

void Point_Widget::removeCallbacks() {
    if (!_active_key.empty() && _callback_id != -1) {
        bool success = _data_manager->removeCallbackFromData(_active_key, _callback_id);
        if (success) {
            _callback_id = -1;
        } else {
            // std::cerr << "Point_Widget: Failed to remove callback for key: " << _active_key << std::endl;
        }
    }
}

void Point_Widget::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (!index.isValid()) {
        return;
    }
    int frame = _point_table_model->getFrameForRow(index.row());
    if (frame != -1) {
        emit frameSelected(frame);
    }
}

void Point_Widget::_populateMoveToPointDataComboBox() {
    ui->moveToPointDataComboBox->clear();
    if (!_data_manager) return;
    std::vector<std::string> point_keys = _data_manager->getKeys<PointData>();
    for (std::string const & key : point_keys) {
        if (key != _active_key) {
            ui->moveToPointDataComboBox->addItem(QString::fromStdString(key));
        }
    }
}

void Point_Widget::_movePointsButton_clicked() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Point_Widget: No frame selected to move points from." << std::endl;
        return;
    }
    int selected_row = selectedIndexes.first().row();
    int frame_to_move = _point_table_model->getFrameForRow(selected_row);

    if (frame_to_move == -1) {
        std::cout << "Point_Widget: Selected row data is invalid (no valid frame)." << std::endl;
        return;
    }

    QString target_key_qstr = ui->moveToPointDataComboBox->currentText();
    if (target_key_qstr.isEmpty()) {
        std::cout << "Point_Widget: No target PointData selected in ComboBox." << std::endl;
        return;
    }
    std::string target_key = target_key_qstr.toStdString();

    auto source_point_data = _data_manager->getData<PointData>(_active_key);
    auto target_point_data = _data_manager->getData<PointData>(target_key);

    if (!source_point_data) {
        std::cerr << "Point_Widget: Source PointData ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_point_data) {
        std::cerr << "Point_Widget: Target PointData ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::vector<Point2D<float>> const & points_to_move = source_point_data->getPointsAtTime(frame_to_move);
    
    if (!points_to_move.empty()) {
        target_point_data->addPointsAtTime(frame_to_move, points_to_move);
    }

    source_point_data->clearPointsAtTime(frame_to_move);

    updateTable();
    _populateMoveToPointDataComboBox();

    std::cout << "Points from frame " << frame_to_move << " moved from " << _active_key << " to " << target_key << std::endl;
}

void Point_Widget::_deletePointsButton_clicked() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Point_Widget: No frame selected to delete points from." << std::endl;
        return;
    }
    int selected_row = selectedIndexes.first().row();
    int frame_to_delete = _point_table_model->getFrameForRow(selected_row);

    if (frame_to_delete == -1) {
        std::cout << "Point_Widget: Selected row data for deletion is invalid (no valid frame)." << std::endl;
        return;
    }

    auto source_point_data = _data_manager->getData<PointData>(_active_key);
    if (!source_point_data) {
        std::cerr << "Point_Widget: Source PointData ('" << _active_key << "') not found for deletion." << std::endl;
        return;
    }

    source_point_data->clearPointsAtTime(frame_to_delete);

    updateTable();
    _populateMoveToPointDataComboBox();

    std::cout << "Points deleted from frame " << frame_to_delete << " in " << _active_key << std::endl;
}

void Point_Widget::_onDataChanged() {
    updateTable();
}

#include "TimeFrame_Table_Widget.hpp"
#include "ui_TimeFrame_Table_Widget.h"

#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QHeaderView>
#include <QResizeEvent>
#include <QTableWidgetItem>

#include <algorithm>
#include <iostream>

TimeFrame_Table_Widget::TimeFrame_Table_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TimeFrame_Table_Widget) {
    ui->setupUi(this);

    // Disable horizontal scrollbar
    ui->timeframe_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Header styling
    ui->timeframe_table->horizontalHeader()->setStretchLastSection(false);

    QFont headerFont = ui->timeframe_table->horizontalHeader()->font();
    headerFont.setPointSize(8);
    ui->timeframe_table->horizontalHeader()->setFont(headerFont);

    QFont tableFont = ui->timeframe_table->font();
    tableFont.setPointSize(9);
    ui->timeframe_table->setFont(tableFont);

    // Uniform row spacing
    ui->timeframe_table->verticalHeader()->setDefaultSectionSize(25);
    ui->timeframe_table->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Alternating row colors, no grid
    ui->timeframe_table->setAlternatingRowColors(true);
    ui->timeframe_table->setShowGrid(false);

    // Dark-mode styling consistent with Feature_Table_Widget
    ui->timeframe_table->setStyleSheet(
            "QTableWidget {"
            "    gridline-color: transparent;"
            "    alternate-background-color: #313131;"
            "    background-color: #2a2a2a;"
            "}"
            "QTableWidget::item:selected {"
            "    background-color: #0078d4;"
            "    color: white;"
            "}"
            "QTableWidget::item:selected:focus {"
            "    background-color: #106ebe;"
            "    color: white;"
            "}");
}

TimeFrame_Table_Widget::~TimeFrame_Table_Widget() {
    delete ui;
}

void TimeFrame_Table_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {

    if (!data_manager) {
        std::cout << "TimeFrame_Table_Widget::setDataManager - Data manager is null" << std::endl;
        return;
    }

    _data_manager = std::move(data_manager);

    // Subscribe to DataManager-level observer so we refresh whenever data changes
    _data_manager->addObserver([this]() {
        _refreshTable();
    });
}

void TimeFrame_Table_Widget::populateTable() {

    if (!_data_manager) {
        return;
    }

    QStringList const columns{QStringLiteral("Name"), QStringLiteral("Samples")};

    ui->timeframe_table->setRowCount(0);
    ui->timeframe_table->setColumnCount(static_cast<int>(columns.size()));
    ui->timeframe_table->setHorizontalHeaderLabels(columns);

    auto const keys = _data_manager->getTimeFrameKeys();

    // Sort keys alphabetically for deterministic display
    std::vector<TimeKey> sortedKeys(keys.begin(), keys.end());
    std::sort(sortedKeys.begin(), sortedKeys.end());

    for (auto const & timeKey : sortedKeys) {
        int const row = ui->timeframe_table->rowCount();
        ui->timeframe_table->insertRow(row);

        // Name column
        auto * nameItem = new QTableWidgetItem(QString::fromStdString(timeKey.str()));
        nameItem->setTextAlignment(Qt::AlignCenter);
        ui->timeframe_table->setItem(row, 0, nameItem);

        // Samples column
        auto timeframe = _data_manager->getTime(timeKey);
        int const sampleCount = timeframe ? timeframe->getTotalFrameCount() : 0;
        auto * samplesItem = new QTableWidgetItem(QString::number(sampleCount));
        samplesItem->setTextAlignment(Qt::AlignCenter);
        ui->timeframe_table->setItem(row, 1, samplesItem);
    }

    // Adjust visible height (up to MAX_VISIBLE_ROWS)
    constexpr int MAX_VISIBLE_ROWS = 6;
    int const rowHeight = ui->timeframe_table->verticalHeader()->defaultSectionSize();
    int const headerHeight = ui->timeframe_table->horizontalHeader()->height();
    int const visibleRows = std::min(ui->timeframe_table->rowCount(), MAX_VISIBLE_ROWS);
    int const totalHeight = (rowHeight * visibleRows) + headerHeight;
    ui->timeframe_table->setMinimumHeight(totalHeight);
    ui->timeframe_table->setMaximumHeight(totalHeight);

    _setAdaptiveColumnWidths();
    updateGeometry();
}

void TimeFrame_Table_Widget::_refreshTable() {
    populateTable();
}

void TimeFrame_Table_Widget::_setAdaptiveColumnWidths() {
    int const totalWidth = ui->timeframe_table->viewport()->width();
    if (totalWidth <= 0) {
        return;
    }

    // 60% for Name, 40% for Samples
    int const nameWidth = static_cast<int>(totalWidth * 0.60);
    int const samplesWidth = totalWidth - nameWidth;

    ui->timeframe_table->setColumnWidth(0, nameWidth);
    ui->timeframe_table->setColumnWidth(1, samplesWidth);
}

void TimeFrame_Table_Widget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);

    if (_is_resizing) {
        return;
    }
    _is_resizing = true;

    _setAdaptiveColumnWidths();

    _is_resizing = false;
}

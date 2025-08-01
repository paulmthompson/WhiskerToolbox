//
// Example of how EventPlotWidget could access table data
// This would be added to EventPlotWidget.cpp in a future implementation
//

// In EventPlotWidget.hpp, add:
/*
private:
    QString _current_table_id;  // Selected table for data
    
private slots:
    void onTableSelectionChanged(const QString& table_id);
*/

// In EventPlotWidget.cpp, add method:
/*
void EventPlotWidget::setTableDataSource(const QString& table_id) {
    _current_table_id = table_id;
    
    if (_table_manager && !table_id.isEmpty()) {
        auto table_view = getTableView(table_id);
        if (table_view) {
            // Example: Get event count data from a table column
            if (table_view->hasColumn("event_count")) {
                auto event_counts = table_view->getColumnValues<int>("event_count");
                
                // Convert to visualization format
                std::vector<std::vector<float>> visualization_data;
                for (size_t i = 0; i < event_counts.size(); ++i) {
                    std::vector<float> row_data;
                    row_data.push_back(static_cast<float>(event_counts[i]));
                    visualization_data.push_back(row_data);
                }
                
                // Pass to OpenGL widget
                if (_opengl_widget) {
                    _opengl_widget->setEventData(visualization_data);
                    emit renderUpdateRequested(getPlotId());
                }
                
                qDebug() << "Loaded" << event_counts.size() << "rows from table" << table_id;
            }
        }
    }
}

void EventPlotWidget::loadTableData() {
    // Alternative approach: Use table data as the primary data source
    // instead of loading from DataManager directly
    
    if (_current_table_id.isEmpty() || !_table_manager) {
        return;
    }
    
    auto table_view = getTableView(_current_table_id);
    if (!table_view) {
        qDebug() << "Table not found:" << _current_table_id;
        return;
    }
    
    // Get available columns from the table
    auto column_names = table_view->getColumnNames();
    
    qDebug() << "Available columns in table" << _current_table_id << ":" << 
                QString::fromStdString(column_names.empty() ? "none" : 
                std::accumulate(column_names.begin() + 1, column_names.end(), 
                column_names[0], [](const std::string& a, const std::string& b) {
                    return a + ", " + b;
                }));
    
    // Use the first column as Y-axis data (or let user select)
    if (!column_names.empty()) {
        // This would depend on the column type - need to check what type it returns
        // For example, if it returns std::vector<float>:
        if (table_view->hasColumn(column_names[0])) {
            try {
                auto column_data = table_view->getColumnValues<std::vector<float>>(column_names[0]);
                
                if (_opengl_widget) {
                    _opengl_widget->setEventData(column_data);
                    emit renderUpdateRequested(getPlotId());
                }
                
                qDebug() << "Successfully loaded" << column_data.size() << 
                            "data points from column" << QString::fromStdString(column_names[0]);
            } catch (const std::exception& e) {
                qDebug() << "Error loading column data:" << e.what();
            }
        }
    }
}
*/

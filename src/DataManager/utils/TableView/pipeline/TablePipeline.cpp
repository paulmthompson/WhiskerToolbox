#include "TablePipeline.hpp"

#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/ComputerRegistryTypes.hpp"
#include "utils/TableView/TableRegistry.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IIntervalSource.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/transforms/PCATransform.hpp"

#include <chrono>
#include <fstream>
#include <iostream>

TablePipeline::TablePipeline(TableRegistry * table_registry, DataManager * data_manager)
    : table_registry_(table_registry),
      data_manager_(data_manager),
      data_manager_extension_(table_registry->getDataManagerExtension()),
      computer_registry_(&table_registry->getComputerRegistry()) {

    if (!table_registry) {
        throw std::invalid_argument("TableRegistry cannot be null");
    }
}

bool TablePipeline::loadFromJson(nlohmann::json const & json_config) {
    try {
        clear();

        // Load metadata if present
        if (json_config.contains("metadata")) {
            metadata_ = json_config["metadata"];
        }

        // Load tables array
        if (!json_config.contains("tables")) {
            std::cerr << "TablePipeline: JSON must contain 'tables' array" << std::endl;
            return false;
        }

        if (!json_config["tables"].is_array()) {
            std::cerr << "TablePipeline: 'tables' must be an array" << std::endl;
            return false;
        }

        for (auto const & table_json: json_config["tables"]) {
            auto config = parseTableConfiguration(table_json);

            // Validate configuration
            std::string validation_error = validateTableConfiguration(config);
            if (!validation_error.empty()) {
                std::cerr << "TablePipeline: Invalid table configuration for '"
                          << config.table_id << "': " << validation_error << std::endl;
                return false;
            }

            tables_.push_back(std::move(config));
        }

        std::cout << "TablePipeline: Loaded " << tables_.size() << " table configurations" << std::endl;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "TablePipeline: Error loading JSON: " << e.what() << std::endl;
        return false;
    }
}

bool TablePipeline::loadFromJsonFile(std::string const & json_file_path) {
    std::ifstream file(json_file_path);
    if (!file.is_open()) {
        std::cerr << "TablePipeline: Cannot open file: " << json_file_path << std::endl;
        return false;
    }

    nlohmann::json json_config;
    try {
        file >> json_config;
    } catch (std::exception const & e) {
        std::cerr << "TablePipeline: Error parsing JSON file: " << e.what() << std::endl;
        return false;
    }

    return loadFromJson(json_config);
}

TablePipelineResult TablePipeline::execute(TablePipelineProgressCallback progress_callback) {
    auto start_time = std::chrono::high_resolution_clock::now();

    TablePipelineResult result;
    result.total_tables = static_cast<int>(tables_.size());

    if (tables_.empty()) {
        result.success = true;
        return result;
    }

    try {
        for (size_t i = 0; i < tables_.size(); ++i) {
            auto const & config = tables_[i];

            // Report progress
            if (progress_callback) {
                int overall_progress = static_cast<int>((i * 100) / tables_.size());
                progress_callback(static_cast<int>(i), config.name, 0, overall_progress);
            }

            // Build the table
            auto table_result = buildTable(config, [&](int columns_done, int total_columns) {
                if (progress_callback) {
                    int table_progress = total_columns > 0 ? (columns_done * 100) / total_columns : 100;
                    int overall_progress = static_cast<int>((i * 100) / tables_.size());
                    progress_callback(static_cast<int>(i), config.name, table_progress, overall_progress);
                }
            });

            result.table_results.push_back(table_result);

            if (table_result.success) {
                result.tables_completed++;
            } else {
                result.success = false;
                result.error_message = "Failed to build table '" + config.table_id + "': " + table_result.error_message;
                break;
            }
        }

        if (result.tables_completed == result.total_tables) {
            result.success = true;
        }

    } catch (std::exception const & e) {
        result.success = false;
        result.error_message = "Exception during pipeline execution: " + std::string(e.what());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    result.total_execution_time_ms = static_cast<double>(duration.count()) / 1000.0;

    return result;
}

TableBuildResult TablePipeline::buildTable(TableConfiguration const & config,
                                           std::function<void(int, int)> progress_callback) {
    auto start_time = std::chrono::high_resolution_clock::now();

    TableBuildResult result;
    result.table_id = config.table_id;
    result.total_columns = static_cast<int>(config.columns.size());

    try {
        if (table_registry_->hasTable(config.table_id)) {
            table_registry_->updateTableInfo(config.table_id, config.name, config.description);
        } else {
            table_registry_->createTable(config.table_id, config.name, config.description);
        }

        // Create TableViewBuilder
        TableViewBuilder builder(data_manager_extension_);

        // Create and set row selector
        auto row_selector = createRowSelector(config.row_selector);
        if (!row_selector) {
            result.error_message = "Failed to create row selector";
            return result;
        }

        RowSelectorType selector_type = parseRowSelectorType(config.row_selector["type"]);
        builder.setRowSelector(std::move(row_selector));

        // Add columns
        for (size_t i = 0; i < config.columns.size(); ++i) {
            auto const & column_json = config.columns[i];

            // Report progress
            if (progress_callback) {
                progress_callback(static_cast<int>(i), result.total_columns);
            }

            // Create column computer
            auto computer = createColumnComputer(column_json, selector_type);
            if (!computer) {
                result.error_message = "Failed to create computer for column: " +
                                       column_json.value("name", "unnamed");
                return result;
            }

            // Add column to builder - this requires template specialization
            std::string column_name = column_json["name"];

            // Get computer info to determine output type
            std::string computer_name = column_json["computer"];
            auto computer_info = computer_registry_->findComputerInfo(computer_name);
            if (!computer_info) {
                result.error_message = "Computer not found: " + computer_name;
                return result;
            }

            // Add column based on output type
            if (!addColumnToBuilder(builder, column_json, std::move(computer))) {
                result.error_message = "Failed to add column to builder: " + column_name;
                return result;
            }

            result.columns_built++;
        }

        // Final progress report
        if (progress_callback) {
            progress_callback(result.total_columns, result.total_columns);
        }

        // Build the table
        TableView table_view = builder.build();

        // Store the built table in TableManager
        if (!table_registry_->storeBuiltTable(config.table_id, std::make_unique<TableView>(std::move(table_view)))) {
            result.error_message = "Failed to store built table in TableManager";
            return result;
        }

        // Apply optional transforms for this base table
        if (!applyTransforms(config)) {
            std::cerr << "TablePipeline: One or more transforms failed for table '" << config.table_id << "'\n";
        }

        result.success = true;

    } catch (std::exception const & e) {
        result.error_message = "Exception during table build: " + std::string(e.what());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    result.build_time_ms = static_cast<double>(duration.count()) / 1000.0;

    return result;
}

void TablePipeline::clear() {
    tables_.clear();
    metadata_ = nlohmann::json{};
}

TableConfiguration TablePipeline::parseTableConfiguration(nlohmann::json const & table_json) {
    TableConfiguration config;

    config.table_id = table_json.value("table_id", "");
    config.name = table_json.value("name", "");
    config.description = table_json.value("description", "");

    if (table_json.contains("row_selector")) {
        config.row_selector = table_json["row_selector"];
    }

    if (table_json.contains("columns") && table_json["columns"].is_array()) {
        for (auto const & column_json: table_json["columns"]) {
            config.columns.push_back(column_json);
        }
    }

    if (table_json.contains("tags") && table_json["tags"].is_array()) {
        for (auto const & tag: table_json["tags"]) {
            if (tag.is_string()) {
                config.tags.push_back(tag);
            }
        }
    }
    if (table_json.contains("transforms") && table_json["transforms"].is_array()) {
        for (auto const & tjson: table_json["transforms"]) {
            TableConfiguration::TransformSpec spec;
            spec.type = tjson.value("type", "");
            if (tjson.contains("parameters") && tjson["parameters"].is_object()) {
                spec.parameters = tjson["parameters"];
            }
            spec.output_table_id = tjson.value("output_table_id", "");
            spec.output_name = tjson.value("output_name", "");
            spec.output_description = tjson.value("output_description", "");
            config.transforms.push_back(std::move(spec));
        }
    }

    return config;
}

bool TablePipeline::applyTransforms(TableConfiguration const & config) {
    if (config.transforms.empty()) return true;

    auto base_id = config.table_id;
    auto base_view_sp = table_registry_->getBuiltTable(base_id);
    if (!base_view_sp) {
        std::cerr << "TablePipeline: Cannot apply transforms, base table not found: " << config.table_id << "\n";
        return false;
    }

    bool all_ok = true;
    for (auto const & t: config.transforms) {
        try {
            if (t.type == "PCA") {
                PCAConfig pc;
                pc.center = t.parameters.value("center", true);
                pc.standardize = t.parameters.value("standardize", false);
                if (t.parameters.contains("include") && t.parameters["include"].is_array()) {
                    for (auto const & s: t.parameters["include"])
                        if (s.is_string()) pc.include.push_back(s);
                }
                if (t.parameters.contains("exclude") && t.parameters["exclude"].is_array()) {
                    for (auto const & s: t.parameters["exclude"])
                        if (s.is_string()) pc.exclude.push_back(s);
                }

                PCATransform pca(pc);
                TableView derived = pca.apply(*base_view_sp);

                // Prepare output id/name
                std::string out_id = t.output_table_id.empty()
                                             ? table_registry_->generateUniqueTableId(config.table_id + "_pca")
                                             : t.output_table_id;
                std::string out_name = t.output_name.empty()
                                               ? config.name + " (PCA)"
                                               : t.output_name;
                std::string out_desc = t.output_description;

                if (!table_registry_->hasTable(out_id)) {
                    table_registry_->createTable(out_id, out_name, out_desc);
                } else {
                    table_registry_->updateTableInfo(out_id, out_name, out_desc);
                }
                if (!table_registry_->storeBuiltTable(out_id, std::make_unique<TableView>(std::move(derived)))) {
                    std::cerr << "TablePipeline: Failed to store transformed table: " << out_id << "\n";
                    all_ok = false;
                }
            } else {
                std::cerr << "TablePipeline: Unknown transform type: " << t.type << "\n";
                all_ok = false;
            }
        } catch (std::exception const & e) {
            std::cerr << "TablePipeline: Transform '" << t.type << "' failed: " << e.what() << "\n";
            all_ok = false;
        }
    }
    return all_ok;
}

std::unique_ptr<IRowSelector> TablePipeline::createRowSelector(nlohmann::json const & row_selector_json) {
    if (!row_selector_json.contains("type")) {
        std::cerr << "TablePipeline: Row selector must have 'type' field" << std::endl;
        return nullptr;
    }

    std::string type = row_selector_json["type"];

    if (type == "interval") {
        // Handle interval row selector creation
        if (row_selector_json.contains("source")) {
            // Source-based interval specification - get intervals from DigitalIntervalSeries
            std::string source_key = row_selector_json["source"];

            // Get the interval source from the DataManagerExtension
            auto interval_source = data_manager_extension_->getIntervalSource(source_key);
            if (!interval_source) {
                std::cerr << "TablePipeline: Cannot resolve interval source: " << source_key << std::endl;
                return nullptr;
            }

            // Get the source's timeframe
            auto source_timeframe = interval_source->getTimeFrame();
            if (!source_timeframe) {
                std::cerr << "TablePipeline: Interval source has no timeframe: " << source_key << std::endl;
                return nullptr;
            }

            // Get all intervals from the source
            auto intervals = interval_source->getIntervalsInRange(
                    TimeFrameIndex(0),
                    TimeFrameIndex(source_timeframe->getTotalFrameCount() - 1),
                    source_timeframe.get());

            if (intervals.empty()) {
                std::cerr << "TablePipeline: No intervals found in source: " << source_key << std::endl;
                return nullptr;
            }

            // Convert Interval objects to TimeFrameInterval objects
            std::vector<TimeFrameInterval> time_frame_intervals;
            time_frame_intervals.reserve(intervals.size());
            for (auto const & interval: intervals) {
                time_frame_intervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
            }

            return std::make_unique<IntervalSelector>(std::move(time_frame_intervals), source_timeframe);

        } else if (row_selector_json.contains("intervals") && row_selector_json["intervals"].is_array()) {
            // Direct interval specification
            std::vector<TimeFrameInterval> intervals;
            std::shared_ptr<TimeFrame> timeFrame;

            // Check if timeframe is specified
            if (row_selector_json.contains("timeframe")) {
                std::string timeframe_key = row_selector_json["timeframe"];
                timeFrame = data_manager_->getTime(TimeKey(timeframe_key));
                if (!timeFrame) {
                    std::cerr << "TablePipeline: Cannot resolve timeframe: " << timeframe_key << std::endl;
                    return nullptr;
                }
            } else {
                // Use default timeframe
                timeFrame = data_manager_->getTime();
                if (!timeFrame) {
                    std::cerr << "TablePipeline: No default timeframe available" << std::endl;
                    return nullptr;
                }
            }

            // Parse interval specifications
            for (auto const & interval_json: row_selector_json["intervals"]) {
                if (interval_json.is_array() && interval_json.size() == 2) {
                    int64_t start = interval_json[0].get<int64_t>();
                    int64_t end = interval_json[1].get<int64_t>();
                    intervals.emplace_back(TimeFrameIndex(start), TimeFrameIndex(end));
                } else if (interval_json.is_object() &&
                           interval_json.contains("start") && interval_json.contains("end")) {
                    int64_t start = interval_json["start"].get<int64_t>();
                    int64_t end = interval_json["end"].get<int64_t>();
                    intervals.emplace_back(TimeFrameIndex(start), TimeFrameIndex(end));
                } else {
                    std::cerr << "TablePipeline: Invalid interval specification in intervals array" << std::endl;
                    return nullptr;
                }
            }

            if (intervals.empty()) {
                std::cerr << "TablePipeline: No valid intervals found in intervals array" << std::endl;
                return nullptr;
            }

            return std::make_unique<IntervalSelector>(std::move(intervals), timeFrame);

        } else {
            std::cerr << "TablePipeline: Interval row selector must have 'source' field or 'intervals' array" << std::endl;
            return nullptr;
        }

    } else if (type == "timestamp") {
        // Handle timestamp row selector creation
        std::vector<TimeFrameIndex> timestamps;
        std::shared_ptr<TimeFrame> timeFrame;

        if (row_selector_json.contains("timestamps") && row_selector_json["timestamps"].is_array()) {
            // Direct timestamp specification
            for (auto const & ts: row_selector_json["timestamps"]) {
                if (ts.is_number_integer()) {
                    timestamps.emplace_back(ts.get<int64_t>());
                } else if (ts.is_number_float()) {
                    timestamps.emplace_back(static_cast<int64_t>(ts.get<double>()));
                }
            }

            // Check if timeframe is specified
            if (row_selector_json.contains("timeframe")) {
                std::string timeframe_key = row_selector_json["timeframe"];
                timeFrame = data_manager_->getTime(TimeKey(timeframe_key));
                if (!timeFrame) {
                    std::cerr << "TablePipeline: Cannot resolve timeframe: " << timeframe_key << std::endl;
                    return nullptr;
                }
            } else {
                // Use default timeframe
                timeFrame = data_manager_->getTime();
                if (!timeFrame) {
                    std::cerr << "TablePipeline: No default timeframe available" << std::endl;
                    return nullptr;
                }
            }

        } else if (row_selector_json.contains("source")) {
            // Source-based timestamp specification
            std::string source_key = row_selector_json["source"];

            // Try to get timestamps from a DigitalEventSeries
            if (auto event_source = data_manager_extension_->getEventSource(source_key)) {
                // Get all event times using the source's timeframe
                auto source_timeframe = event_source->getTimeFrame();
                if (source_timeframe && source_timeframe->getTotalFrameCount() > 0) {
                    TimeFrameIndex start_idx(0);
                    TimeFrameIndex end_idx(source_timeframe->getTotalFrameCount() - 1);
                    auto event_times = event_source->getEventsInRange(start_idx,
                                                                      end_idx,
                                                                      *source_timeframe);

                    for (auto time: event_times) {
                        timestamps.emplace_back(static_cast<int64_t>(time));
                    }
                    timeFrame = source_timeframe;
                } else {
                    std::cerr << "TablePipeline: Event source has no timeframe or no data: " << source_key << std::endl;
                    return nullptr;
                }

            } else {
                // Try to get specific TimeFrame
                timeFrame = data_manager_->getTime(TimeKey(source_key));
                if (timeFrame) {
                    // Generate timestamps from the TimeFrame
                    int frame_count = timeFrame->getTotalFrameCount();
                    timestamps.reserve(static_cast<size_t>(frame_count));
                    for (int i = 0; i < frame_count; ++i) {
                        timestamps.emplace_back(timeFrame->getTimeAtIndex(TimeFrameIndex(i)));
                    }
                } else {
                    std::cerr << "TablePipeline: Cannot resolve timestamp source: " << source_key << std::endl;
                    return nullptr;
                }
            }
        } else {
            std::cerr << "TablePipeline: Timestamp row selector must have 'timestamps' array or 'source' field" << std::endl;
            return nullptr;
        }

        if (timestamps.empty()) {
            std::cerr << "TablePipeline: No timestamps found for timestamp row selector" << std::endl;
            return nullptr;
        }

        if (!timeFrame) {
            std::cerr << "TablePipeline: No TimeFrame available for timestamp row selector" << std::endl;
            return nullptr;
        }

        return std::make_unique<TimestampSelector>(std::move(timestamps), timeFrame);

    } else if (type == "index") {
        // TODO: Implement index row selector creation
        std::cerr << "TablePipeline: Index row selector not yet implemented" << std::endl;
        return nullptr;

    } else {
        std::cerr << "TablePipeline: Unknown row selector type: " << type << std::endl;
        return nullptr;
    }
}

std::unique_ptr<IComputerBase> TablePipeline::createColumnComputer(nlohmann::json const & column_json,
                                                                   RowSelectorType row_selector_type) {
    (void) row_selector_type;// Suppress unused parameter warning
    if (!column_json.contains("computer")) {
        std::cerr << "TablePipeline: Column must have 'computer' field" << std::endl;
        return nullptr;
    }

    std::string computer_name = column_json["computer"];

    // Resolve data source
    DataSourceVariant data_source;
    std::string data_source_name;
    if (column_json.contains("data_source")) {
        if (column_json["data_source"].is_string()) {
            data_source_name = column_json["data_source"];
        }
        data_source = resolveDataSource(column_json["data_source"]);
    }

    // Get parameters
    std::map<std::string, std::string> parameters;
    if (column_json.contains("parameters") && column_json["parameters"].is_object()) {
        for (auto const & [key, value]: column_json["parameters"].items()) {
            if (value.is_string()) {
                parameters[key] = value;
            } else {
                parameters[key] = value.dump();
            }
        }
    }
    
    // Add data source name as a special parameter for factories that need it
    if (!data_source_name.empty()) {
        parameters["__source_name__"] = data_source_name;
    }

    // Check if this is a multi-output computer
    auto computer_info = computer_registry_->findComputerInfo(computer_name);
    if (!computer_info) {
        std::cerr << "TablePipeline: Computer not found: " << computer_name << std::endl;
        return nullptr;
    }

    // Create computer using the appropriate registry method
    if (computer_info->isMultiOutput) {
        return computer_registry_->createMultiComputer(computer_name, data_source, parameters);
    } else {
        return computer_registry_->createComputer(computer_name, data_source, parameters);
    }
}

DataSourceVariant TablePipeline::resolveDataSource(nlohmann::json const & data_source_json) {
    if (data_source_json.is_string()) {
        // Simple key reference - resolve directly through DataManagerExtension
        std::string key = data_source_json;

        // Try different interface types
        if (auto analog_source = data_manager_extension_->getAnalogSource(key)) {
            return analog_source;
        }
        if (auto event_source = data_manager_extension_->getEventSource(key)) {
            return event_source;
        }
        if (auto interval_source = data_manager_extension_->getIntervalSource(key)) {
            return interval_source;
        }
        if (auto line_source = data_manager_extension_->getLineSource(key)) {
            return line_source;
        }

        std::cerr << "TablePipeline: Could not resolve data source: " << key << std::endl;
        return {};

    } else if (data_source_json.is_object()) {
        // Complex data source with adapter
        std::string key = data_source_json.value("key", "");
        std::string adapter = data_source_json.value("adapter", "");

        if (key.empty() || adapter.empty()) {
            std::cerr << "TablePipeline: Data source object must have 'key' and 'adapter' fields" << std::endl;
            return {};
        }

        // TODO: Implement adapter-based data source resolution
        // This would use the ComputerRegistry's adapter system
        std::cerr << "TablePipeline: Adapter-based data sources not yet implemented" << std::endl;
        return {};

    } else {
        std::cerr << "TablePipeline: Invalid data source specification" << std::endl;
        return {};
    }
}

RowSelectorType TablePipeline::parseRowSelectorType(std::string const & type_string) {
    if (type_string == "interval") {
        return RowSelectorType::IntervalBased;
    } else if (type_string == "timestamp") {
        return RowSelectorType::Timestamp;
    } else if (type_string == "index") {
        return RowSelectorType::Index;
    } else {
        std::cerr << "TablePipeline: Unknown row selector type: " << type_string << std::endl;
        return RowSelectorType::IntervalBased;// Default fallback
    }
}

std::string TablePipeline::validateTableConfiguration(TableConfiguration const & config) {
    if (config.table_id.empty()) {
        return "table_id cannot be empty";
    }

    if (config.name.empty()) {
        return "name cannot be empty";
    }

    if (config.columns.empty()) {
        return "table must have at least one column";
    }

    // Validate row selector
    if (!config.row_selector.contains("type")) {
        return "row_selector must have 'type' field";
    }

    // Validate columns
    for (size_t i = 0; i < config.columns.size(); ++i) {
        auto const & column = config.columns[i];

        if (!column.contains("name")) {
            return "column " + std::to_string(i) + " missing 'name' field";
        }

        if (!column.contains("computer")) {
            return "column " + std::to_string(i) + " missing 'computer' field";
        }

        if (!column.contains("data_source")) {
            return "column " + std::to_string(i) + " missing 'data_source' field";
        }
    }

    return "";// Valid
}

// Helper template function to try adding a column with a specific type
template<typename T>
bool tryAddColumnWithType(TableViewBuilder & builder,
                          std::string const & column_name,
                          std::unique_ptr<IComputerBase> & computer) {
    // First try multi-output computer
    auto multi_wrapper = dynamic_cast<MultiComputerWrapper<T> *>(computer.get());
    if (multi_wrapper) {
        auto multi_computer = multi_wrapper->releaseComputer();
        builder.addColumns<T>(column_name, std::move(multi_computer));
        computer.reset();// Mark as consumed
        return true;
    }

    // Try single-output computer wrapper
    auto single_wrapper = dynamic_cast<ComputerWrapper<T> *>(computer.get());
    if (single_wrapper) {
        auto typed_computer = single_wrapper->releaseComputer();
        builder.addColumn(column_name, std::move(typed_computer));
        computer.reset();// Mark as consumed
        return true;
    }

    return false;
}

// Helper function to add column to builder based on type
bool TablePipeline::addColumnToBuilder(TableViewBuilder & builder,
                                       nlohmann::json const & column_json,
                                       std::unique_ptr<IComputerBase> computer) {
    std::string column_name = column_json["name"];

    // Try each supported type in order
    // Start with the most common types for better performance

    // Scalar types
    if (tryAddColumnWithType<double>(builder, column_name, computer)) return true;
    if (tryAddColumnWithType<float>(builder, column_name, computer)) return true;
    if (tryAddColumnWithType<int64_t>(builder, column_name, computer)) return true;
    if (tryAddColumnWithType<int>(builder, column_name, computer)) return true;
    if (tryAddColumnWithType<bool>(builder, column_name, computer)) return true;

    // Vector types
    if (tryAddColumnWithType<std::vector<double>>(builder, column_name, computer)) return true;
    if (tryAddColumnWithType<std::vector<float>>(builder, column_name, computer)) return true;
    if (tryAddColumnWithType<std::vector<int>>(builder, column_name, computer)) return true;
    if (tryAddColumnWithType<std::vector<TimeFrameIndex>>(builder, column_name, computer)) return true;

    // If we get here, the computer type is not supported
    std::cerr << "TablePipeline: Unsupported computer output type for column: " << column_name << std::endl;
    return false;
}

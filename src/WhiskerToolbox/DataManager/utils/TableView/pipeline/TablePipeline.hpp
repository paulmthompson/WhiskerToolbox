#ifndef TABLE_PIPELINE_HPP
#define TABLE_PIPELINE_HPP

#include "DataManager.hpp"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/TableInfo.hpp"
#include "utils/TableView/TableRegistry.hpp"

#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

// Forward declarations
class TableManager;
class DataManager;
class ComputerRegistry;
class DataManagerExtension;

/**
 * @brief Progress callback for table pipeline execution
 * 
 * @param table_index Current table being built (0-based)
 * @param table_name Name of the current table
 * @param table_progress Progress of current table (0-100)
 * @param overall_progress Overall pipeline progress (0-100)
 */
using TablePipelineProgressCallback = std::function<void(int table_index, std::string const& table_name, int table_progress, int overall_progress)>;

/**
 * @brief Represents a single table configuration in a table pipeline
 */
struct TableConfiguration {
    std::string table_id;                       // Unique identifier for this table
    std::string name;                           // Human-readable table name
    std::string description;                    // Optional description
    nlohmann::json row_selector;               // Row selector configuration
    std::vector<nlohmann::json> columns;       // Column configurations
    
    // Optional metadata
    std::vector<std::string> tags;              // Tags for organization
    struct TransformSpec {
        std::string type;                       // e.g., "PCA"
        nlohmann::json parameters;              // transform-specific params
        std::string output_table_id;            // optional explicit ID
        std::string output_name;                // optional display name
        std::string output_description;         // optional description
    };
    std::vector<TransformSpec> transforms;      // Optional transforms to apply to this table
};

/**
 * @brief Represents execution result of a table build
 */
struct TableBuildResult {
    bool success = false;
    std::string error_message;
    std::string table_id;
    double build_time_ms = 0.0;
    int columns_built = 0;
    int total_columns = 0;
};

/**
 * @brief Represents execution result of a table pipeline
 */
struct TablePipelineResult {
    bool success = false;
    std::string error_message;
    std::vector<TableBuildResult> table_results;
    double total_execution_time_ms = 0.0;
    int tables_completed = 0;
    int total_tables = 0;
};

/**
 * @brief Factory and execution engine for table construction pipelines
 */
class TablePipeline {
public:
    /**
     * @brief Construct a new Table Pipeline object
     * 
     * @param table_manager Pointer to the table manager
     * @param data_manager Pointer to the data manager
     */
    TablePipeline(TableRegistry* table_registry, DataManager* data_manager);

    /**
     * @brief Load pipeline configuration from JSON
     * 
     * @param json_config JSON configuration object
     * @return true if configuration was loaded successfully
     * @return false if there were errors in the configuration
     */
    bool loadFromJson(nlohmann::json const& json_config);

    /**
     * @brief Load pipeline configuration from JSON file
     * 
     * @param json_file_path Path to JSON configuration file
     * @return true if configuration was loaded successfully
     * @return false if file couldn't be read or parsed
     */
    bool loadFromJsonFile(std::string const& json_file_path);

    /**
     * @brief Execute the loaded pipeline
     * 
     * @param progress_callback Optional progress callback
     * @return TablePipelineResult containing execution results
     */
    TablePipelineResult execute(TablePipelineProgressCallback progress_callback = nullptr);

    /**
     * @brief Execute a single table configuration
     * 
     * @param config The table configuration to build
     * @param progress_callback Optional progress callback for column building
     * @return TableBuildResult containing build results
     */
    TableBuildResult buildTable(TableConfiguration const& config, 
                               std::function<void(int, int)> progress_callback = nullptr);

    /**
     * @brief Get the loaded table configurations
     * 
     * @return Vector of table configurations
     */
    std::vector<TableConfiguration> const& getTableConfigurations() const { return tables_; }

    /**
     * @brief Clear all loaded configurations
     */
    void clear();

private:
    TableRegistry* table_registry_;
    DataManager* data_manager_;
    std::shared_ptr<DataManagerExtension> data_manager_extension_;
    ComputerRegistry* computer_registry_;

    std::vector<TableConfiguration> tables_;
    nlohmann::json metadata_;

    /**
     * @brief Parse a table configuration from JSON
     * 
     * @param table_json JSON object for a single table
     * @return TableConfiguration structure
     */
    TableConfiguration parseTableConfiguration(nlohmann::json const& table_json);

    /**
     * @brief Create a row selector from JSON configuration
     * 
     * @param row_selector_json JSON configuration for row selector
     * @return Unique pointer to created row selector, or nullptr on failure
     */
    std::unique_ptr<IRowSelector> createRowSelector(nlohmann::json const& row_selector_json);

    /**
     * @brief Create a column computer from JSON configuration
     * 
     * @param column_json JSON configuration for a column
     * @param row_selector_type The type of row selector being used
     * @return Unique pointer to created computer base, or nullptr on failure
     */
    std::unique_ptr<IComputerBase> createColumnComputer(nlohmann::json const& column_json,
                                                        RowSelectorType row_selector_type);

    /**
     * @brief Resolve a data source from JSON configuration
     * 
     * @param data_source_json JSON specification of data source
     * @return DataSourceVariant containing the resolved source, or empty on failure
     */
    DataSourceVariant resolveDataSource(nlohmann::json const& data_source_json);

    /**
     * @brief Parse row selector type from string
     * 
     * @param type_string String representation of row selector type
     * @return RowSelectorType enum value
     */
    RowSelectorType parseRowSelectorType(std::string const& type_string);

    /**
     * @brief Validate table configuration
     * 
     * @param config Table configuration to validate
     * @return Error message if invalid, empty string if valid
     */
    std::string validateTableConfiguration(TableConfiguration const& config);

    bool addColumnToBuilder(TableViewBuilder& builder, 
                                      nlohmann::json const& column_json,
                                      std::unique_ptr<IComputerBase> computer);

    bool applyTransforms(TableConfiguration const& config);
};

#endif // TABLE_PIPELINE_HPP

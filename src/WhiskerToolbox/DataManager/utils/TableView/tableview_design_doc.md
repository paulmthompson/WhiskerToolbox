Design Document: The TableView System
Version: 1.0
Date: July 9, 2025
Author: Gemini

1. Introduction & Goals
This document outlines the design for a flexible and high-performance TableView system for data analysis in C++. The primary goal is to provide a unified, expressive interface for creating tabular views of heterogeneous, time-series data (e.g., AnalogData, PointData, Intervals) for tasks such as serialization (CSV), plotting, and input to machine learning libraries.

The design prioritizes the following key principles:

Efficiency: Avoids unnecessary computation and memory allocation through lazy, batch-oriented evaluation. Expensive operations like time-to-index conversion are performed once and cached.

Flexibility: Allows columns to be defined from raw data, lagged/leaded data, complex reductions (e.g., mean over an interval), or transformations of other columns.

Extensibility: New data types and computations can be integrated without modifying the core TableView logic by adding new adapters and computer strategies.

Decoupling: The definition of a table is separate from its computation. The underlying raw data storage is not duplicated.

Usability: A fluent Builder pattern provides a clean and readable API for constructing complex table definitions.

2. Core Architectural Concepts
The system is built on a combination of established design patterns to achieve its goals.

Adapter Pattern: Complex or non-contiguous data structures (like PointData) are wrapped in adapters that expose them through a unified IAnalogSource interface, presenting them as simple, contiguous series of values.

Strategy Pattern: The logic for how a column is computed is encapsulated in IColumnComputer objects. This allows us to have different strategies for direct access, interval reductions, or functional transformations.

Builder Pattern: A TableViewBuilder provides a step-by-step, fluent API for constructing a TableView object, simplifying the setup of complex configurations.

Lazy Evaluation & Caching: A Column's data is not computed until it is first requested. Once computed, the result is cached (materialized) as a contiguous vector for all subsequent accesses. Intermediate computational steps, like converting row timestamps to data indices (ExecutionPlan), are also cached and shared between columns.

3. Key Components & Class Definitions
This section details the primary classes that form the TableView ecosystem.

3.1. IAnalogSource (The Unified Data Interface)
This abstract base class is the cornerstone of the system's flexibility. It provides a common interface for any data that can be treated as a simple array of doubles.

// Interface for any data source that can be viewed as an analog signal.
class IAnalogSource {
public:
    virtual ~IAnalogSource() = default;

    // Gets the ID of the TimeFrame the data belongs to.
    virtual int getTimeFrameId() const = 0;
    
    // Gets the total number of samples in the source.
    virtual size_t size() const = 0;

    // Provides a view over the data.
    // This may trigger a one-time lazy materialization for non-contiguous sources.
    virtual std::span<const double> getDataSpan() = 0;
};

Implementations: AnalogData (physical), PointComponentAdapter (virtual), TensorSliceAdapter (virtual), etc.

3.2. DataManager (The Source Factory)
The DataManager acts as a facade and a factory. It manages the lifecycle of raw data objects and is responsible for creating and caching IAnalogSource adapters.

class DataManager {
public:
    // Unified access point for all data sources.
    // Name can be "LFP" for physical data or "Spikes.x" for a virtual source.
    std::shared_ptr<IAnalogSource> getAnalogSource(const std::string& name);

    // ... methods to load/manage raw AnalogData, PointData, etc. ...
private:
    // Caches for raw data
    std::map<std::string, std::shared_ptr<AnalogData>> m_analogData;
    std::map<std::string, std::shared_ptr<PointData>> m_pointData;

    // Cache for adapter objects to ensure reuse and correct lifetime.
    mutable std::map<std::string, std::shared_ptr<IAnalogSource>> m_dataSourceCache;
};

3.3. IRowSelector (Row Definition)
This interface defines what constitutes a "row" in the table.

// Defines the source and number of rows for the table.
class IRowSelector {
public:
    virtual ~IRowSelector() = default;
    virtual size_t getRowCount() const = 0;
    // ... other methods to access row-defining data (e.g., getTimestamps())
};

// Example Implementations:
class IndexSelector : public IRowSelector { /* uses std::vector<size_t> */ };
class TimestampSelector : public IRowSelector { /* uses std::vector<double> */ };
class IntervalSelector : public IRowSelector { /* uses std::vector<Interval> */ };

3.4. ExecutionPlan (Shared Intermediate Computation)
A simple data object that holds the result of an expensive intermediate calculation, typically the mapping of row definitions to specific data array indices.

// Holds a cached, reusable access pattern for a specific data source.
class ExecutionPlan {
public:
    // Example: For interval reductions, this would hold start/end index pairs.
    const std::vector<std::pair<size_t, size_t>>& getIndexPairs() const;
    
    // Example: For direct access, this would hold single indices.
    const std::vector<size_t>& getIndices() const;
};

3.5. IColumnComputer (Column Computation Strategy)
This interface defines the strategy for computing all values in a column in a single batch operation.

class IColumnComputer {
public:
    virtual ~IColumnComputer() = default;

    // The core batch computation method.
    virtual std::vector<double> compute(const ExecutionPlan& plan) const = 0;

    // Declares dependencies on other columns (for transformed columns).
    virtual std::vector<std::string> getDependencies() const { return {}; }
    
    // Declares the required data source.
    virtual std::string getSourceDependency() const = 0;
};

Implementations: DirectAccessComputer, IntervalReductionComputer, TransformComputer.

3.6. Column & TableView (The Core Objects)
The TableView orchestrates the process, while the Column holds the state for a single column.

class TableView; // Forward declaration

class Column {
public:
    // Gets a span over the column's data. Triggers computation if not materialized.
    std::span<const double> getSpan(TableView* table);
private:
    friend class TableViewBuilder;
    std::string m_name;
    std::unique_ptr<IColumnComputer> m_computer;
    std::variant<std::monostate, std::vector<double>> m_cache;
};

class TableView {
public:
    size_t getRowCount() const;
    size_t getColumnCount() const;
    std::span<const double> getColumnSpan(const std::string& name);

private:
    friend class TableViewBuilder;
    friend class Column; // Allows Column to access the plan cache

    // Gets or creates the ExecutionPlan for a given data source requirement.
    const ExecutionPlan& getExecutionPlanFor(const std::string& sourceName);

    std::unique_ptr<IRowSelector> m_rowSelector;
    std::vector<std::shared_ptr<Column>> m_columns;
    std::map<std::string, size_t> m_colNameToIndex;
    
    // Caches ExecutionPlans, keyed by a unique source identifier.
    std::map<std::string, ExecutionPlan> m_planCache;
};

4. Workflow & Usage Example
4.1. Workflow: Lazy Computation
A user requests data from a column for the first time via tableView.getColumnSpan("MyColumn").

The TableView finds the corresponding Column object and sees its cache is empty.

The TableView inspects the column's IColumnComputer to determine its dependencies (e.g., "I need AnalogData source 'LFP'").

It checks its m_planCache for a pre-computed ExecutionPlan for the 'LFP' source based on the table's RowSelector.

If the plan is not found: The TableView generates it by converting the row definitions (e.g., timestamps) to indices in the 'LFP' array. The new plan is stored in m_planCache.

The cached (or newly created) ExecutionPlan is passed to the column's IColumnComputer::compute() method.

The computer performs its batch operation (e.g., calculating the mean for every interval) and returns a std::vector<double>.

This vector is moved into the Column's internal cache, materializing it.

A std::span over the newly materialized data is returned to the user.

All subsequent calls to getColumnSpan("MyColumn") will find the cached data and immediately return a span.

4.2. Usage Example (Client Code)
This demonstrates how a user would construct and use a TableView.

void example_usage(DataManager& dataManager, const std::vector<Interval>& spikeBursts) {
    // 1. Create a builder and define the rows.
    TableViewBuilder builder;
    builder.setRowSource(std::make_unique<IntervalSelector>(spikeBursts));

    // 2. Get unified source handles from the DataManager.
    auto source_lfp = dataManager.getAnalogSource("LFP");          // Physical source
    auto source_spikes_x = dataManager.getAnalogSource("Spikes.x"); // Virtual source

    // 3. Define columns with different computation strategies.
    builder.addColumn("LFP_Mean", 
        std::make_unique<IntervalReductionComputer>(source_lfp, Reduction::Mean));
        
    builder.addColumn("SpikeX_StdDev", 
        std::make_unique<IntervalReductionComputer>(source_spikes_x, Reduction::StdDev));

    // Example of a transformed column.
    // This computer would take "LFP_Mean" as a dependency.
    builder.addColumn("LFP_Mean_Sq", 
        std::make_unique<TransformComputer>([](double v) { return v * v; }, "LFP_Mean"));

    // 4. Build the final, lazy table object.
    TableView table = builder.build();

    // 5. Use the table. Computation is triggered here.
    std::cout << "Table has " << table.getRowCount() << " rows." << std::endl;
    
    // Get the LFP_Mean column (this triggers its computation).
    std::span<const double> lfp_means = table.getColumnSpan("LFP_Mean");

    // Get the SpikeX_StdDev column (this triggers its computation, but reuses the ExecutionPlan).
    std::span<const double> spike_x_stds = table.getColumnSpan("SpikeX_StdDev");

    // Pass to a machine learning library (e.g., Armadillo)
    // arma::vec arma_lfp_means(lfp_means.data(), lfp_means.size());
}

5. Future Considerations
Error Handling: Implement robust error handling for cases like missing data sources, failed transformations, or mismatched TimeFrames.

Multi-threading: The computation of independent columns is highly parallelizable. The TableView could be extended with a materializeAll() method that computes multiple columns in parallel.

Heterogeneous Column Types: While this design focuses on double, the Column's cache (std::variant) and IColumnComputer could be templated to support other types like int, std::string, etc.

Write-back: The current design is read-only. A mechanism for writing data from a TableView back to a DataManager source could be considered.
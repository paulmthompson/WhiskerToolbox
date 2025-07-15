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

A2. Proposed Design Changes
A2.1. Heterogeneous Columns via Type Erasure
To support multiple data types, we will introduce templates and a non-templated base class to erase the type information at the TableView level.

1. IColumn (New Base Interface):
A non-templated base interface that TableView will use to manage all columns polymorphically.

class IColumn {
public:
    virtual ~IColumn() = default;
    virtual const std::string& getName() const = 0;
    virtual const std::type_info& getType() const = 0;
    // A method to trigger computation without exposing the type.
    virtual void materialize(TableView* table) = 0;
};

2. Column<T> (Templated Class):
The original Column class will be converted to a template and will inherit from IColumn.

template<typename T>
class Column : public IColumn {
public:
    // Returns a span for contiguous types, or a direct reference for complex types.
    const std::vector<T>& getValues(TableView* table);

    // IColumn overrides
    const std::string& getName() const override { return m_name; }
    const std::type_info& getType() const override { return typeid(T); }
    void materialize(TableView* table) override;

private:
    std::string m_name;
    std::unique_ptr<IColumnComputer<T>> m_computer;
    std::variant<std::monostate, std::vector<T>> m_cache;
};

3. IColumnComputer<T> (Templated Interface):
The computer interface must also be templated to return vectors of the correct type.

template<typename T>
class IColumnComputer {
public:
    virtual ~IColumnComputer() = default;
    virtual std::vector<T> compute(const ExecutionPlan& plan) const = 0;
    // ... other methods remain the same
};

4. TableView Modifications:
The TableView will now hold std::vector<std::shared_ptr<IColumn>>. The getColumnSpan method will be replaced with a templated getColumnValues method that performs a dynamic_cast to retrieve the correctly typed column.

class TableView {
public:
    template<typename T>
    const std::vector<T>& getColumnValues(const std::string& name);
private:
    std::vector<std::shared_ptr<IColumn>> m_columns;
    // ... rest of TableView
};

template<typename T>
const std::vector<T>& TableView::getColumnValues(const std::string& name) {
    // 1. Find the IColumn pointer by name.
    // 2. dynamic_cast the IColumn* to a Column<T>*.
    // 3. If cast succeeds, call getValues() on the typed column.
    // 4. If cast fails, throw an exception for type mismatch.
}

A2.2. Event-Based Data Source and Computer
To handle DigitalEventSeries, we introduce a new data source interface and a corresponding computer.

1. IEventSource (New Data Source Interface):
An interface for data that consists of a sorted list of timestamps or indices.

class IEventSource {
public:
    virtual ~IEventSource() = default;
    virtual int getTimeFrameId() const = 0;
    // Returns a span over the sorted event indices/timestamps.
    virtual std::span<const TimeFrameIndex> getEvents() const = 0;
};

2. DigitalEventSeries (New Data Type):
A concrete class holding event data and implementing the IEventSource interface.

3. DataManager Extension:
The DataManager will be extended with a getEventSource(const std::string& name) method.

4. EventInIntervalComputer<T> (New Column Computer):
A new, versatile computer that works with an IEventSource. It can be configured to perform different operations.

enum class EventOperation { Presence, Count, Gather };

template<typename T>
class EventInIntervalComputer : public IColumnComputer<T> {
public:
    EventInIntervalComputer(std::shared_ptr<IEventSource> source, EventOperation op);
    
    std::vector<T> compute(const ExecutionPlan& plan) const override {
        // Get event list from the IEventSource
        auto all_events = m_source->getEvents();
        // Get interval index pairs from the ExecutionPlan
        auto intervals = plan.getIndexPairs();
        
        std::vector<T> results;
        // For each interval, perform an efficient search (e.g., binary search)
        // on the event list to find events within the interval's bounds.
        // Based on m_operation, push back a bool, int, or vector<TimeFrameIndex>
        // into the results vector. This requires template specialization for the logic.
        
        return results;
    }
private:
    std::shared_ptr<IEventSource> m_source;
    EventOperation m_operation;
};

The implementation of compute will use if constexpr or template specialization to handle the different return types (bool, int, std::vector<TimeFrameIndex>) required by the EventOperation.

A3. Updated Usage Example
void new_usage_example(DataManager& dataManager, const std::vector<Interval>& intervals) {
    TableViewBuilder builder;
    builder.setRowSource(std::make_unique<IntervalSelector>(intervals));

    auto event_source = dataManager.getEventSource("MyEvents");

    // Add columns with different types and computers.
    builder.addColumn<bool>("EventPresence", 
        std::make_unique<EventInIntervalComputer<bool>>(event_source, EventOperation::Presence));
        
    builder.addColumn<int>("EventCount", 
        std::make_unique<EventInIntervalComputer<int>>(event_source, EventOperation::Count));

    builder.addColumn<std::vector<TimeFrameIndex>>("GatheredEvents",
        std::make_unique<EventInIntervalComputer<std::vector<TimeFrameIndex>>>(event_source, EventOperation::Gather));

    TableView table = builder.build();

    // Retrieve data with type-safe accessors.
    const auto& counts = table.getColumnValues<int>("EventCount");
    const auto& gathered = table.getColumnValues<std::vector<TimeFrameIndex>>("GatheredEvents");

    std::cout << "Count for first interval: " << counts[0] << std::endl;
}

This addendum makes the TableView system significantly more powerful, allowing it to truly manage heterogeneous data while maintaining its core principles of lazy evaluation and efficiency.

Addendum 2: Support for Gathering Analog Data Slices
Date: July 15, 2025

This addendum addresses the requirement to create columns where each cell contains a window of an analog time series, corresponding to a row's interval.

A4. Problem Statement
A common analysis task is to collect snippets of a continuous signal (e.g., LFP or EEG) around specific events or within defined intervals. The desired output is a column where each cell is a std::vector<float> or std::vector<double>. The existing architecture, as modified by Addendum 1, supports the storage of such a column (Column<std::vector<double>>). What is missing is the specific "computer" strategy to perform this gathering operation.

A5. Proposed Design Changes
No changes are needed for the core architecture (IColumn, TableView, etc.). We only need to define a new, specialized IColumnComputer.

1. AnalogSliceGathererComputer (New Column Computer):
This new computer strategy is responsible for iterating through an ExecutionPlan of interval index pairs and, for each pair, copying the corresponding slice of data from an IAnalogSource into a new vector.

// This computer's output type is a vector of vectors.
// The inner vector can be float or double.
template<typename T = double>
class AnalogSliceGathererComputer : public IColumnComputer<std::vector<T>> {
public:
    AnalogSliceGathererComputer(std::shared_ptr<IAnalogSource> source) 
        : m_source(std::move(source)) {}

    // The core computation method.
    std::vector<std::vector<T>> compute(const ComputeContext& context) const override {
        // Get a view over the entire raw data source once.
        std::span<const double> raw_data = m_source->getDataSpan();
        
        // Get the list of [start, end] index pairs from the shared plan.
        const auto& index_pairs = context.getPlan().getIndexPairs();

        // This is our final result: a vector of vectors.
        std::vector<std::vector<T>> results;
        results.reserve(index_pairs.size());

        for (const auto& pair : index_pairs) {
            size_t start_index = pair.first;
            size_t count = pair.second - pair.first;

            // Create a view of the slice from the raw data.
            auto slice_view = raw_data.subspan(start_index, count);

            // Copy the data from the slice view into a new vector of type T.
            // This constructs the vector for the cell.
            results.emplace_back(slice_view.begin(), slice_view.end());
        }

        return results;
    }

private:
    std::shared_ptr<IAnalogSource> m_source;
};

A6. Updated Usage Example
This example shows how to create a column of LFP snippets.

void waveform_gathering_example(DataManager& dataManager, const std::vector<Interval>& intervals) {
    TableViewBuilder builder;
    builder.setRowSource(std::make_unique<IntervalSelector>(intervals));

    // Get the source for the LFP data.
    auto lfp_source = dataManager.getAnalogSource("LFP");

    // Add a column to gather the LFP data within each interval.
    // The column's type will be std::vector<double>.
    builder.addColumn<std::vector<double>>("LFP_Snippets",
        std::make_unique<AnalogSliceGathererComputer<double>>(lfp_source));

    TableView table = builder.build();

    // Retrieve the materialized column data.
    // `lfp_waveforms` is a const reference to a std::vector<std::vector<double>>.
    const auto& lfp_waveforms = table.getColumnValues<std::vector<double>>("LFP_Snippets");

    // We can now work with the gathered data.
    if (!lfp_waveforms.empty()) {
        std::cout << "First LFP snippet has " << lfp_waveforms[0].size() << " samples." << std::endl;
    }
}

Addendum 3: Support for Statistical Standardization
Date: July 15, 2025

This addendum addresses the requirement to create a column by standardizing another numerical column (Z-scoring).

A7. Problem Statement
A frequent need in statistical analysis is to normalize data by centering it on its mean and scaling it by its standard deviation. This operation requires a full-column transformation: the statistics (mean, stddev) must be computed for the entire source column before the element-wise transformation can be applied.

A8. Proposed Design Changes
The revised IColumnComputer interface using ComputeContext is perfectly suited for this task. We only need to add a new "computer" strategy.

1. StandardizeComputer (New Column Computer):
This computer takes a single column name as a dependency. Its compute method uses the ComputeContext to fetch the materialized data of its dependency, calculates the necessary statistics, and then performs the transformation.

// This computer takes a numerical column and produces a double column.
template<typename T>
class StandardizeComputer : public IColumnComputer<double> {
public:
    StandardizeComputer(std::string dependency_col_name) 
        : m_dependency(std::move(dependency_col_name)) {}

    std::vector<std::string> getDependencies() const override {
        return {m_dependency};
    }

    std::vector<double> compute(const ComputeContext& context) const override {
        // 1. Get the materialized data of the dependency column.
        const auto& source_values = context.getDependencyValues<T>(m_dependency);

        if (source_values.empty()) {
            return {};
        }

        // 2. Calculate mean and standard deviation.
        double sum = std::accumulate(source_values.begin(), source_values.end(), 0.0);
        double mean = sum / source_values.size();
        
        double sq_sum = std::inner_product(source_values.begin(), source_values.end(), source_values.begin(), 0.0);
        double stddev = std::sqrt(sq_sum / source_values.size() - mean * mean);

        // Handle case of zero standard deviation to avoid division by zero.
        if (stddev < 1e-9) {
            return std::vector<double>(source_values.size(), 0.0);
        }

        // 3. Apply the standardization formula to each element.
        std::vector<double> results;
        results.reserve(source_values.size());
        for (const auto& val : source_values) {
            results.push_back((val - mean) / stddev);
        }

        return results;
    }

private:
    std::string m_dependency;
};

A9. Updated Usage Example
This example shows how to create a standardized version of the "LFP_Mean" column.

void standardize_example(DataManager& dataManager, const std::vector<Interval>& intervals) {
    TableViewBuilder builder;
    builder.setRowSource(std::make_unique<IntervalSelector>(intervals));

    auto lfp_source = dataManager.getAnalogSource("LFP");

    // 1. Add the base column we want to standardize.
    builder.addColumn<double>("LFP_Mean", 
        std::make_unique<IntervalReductionComputer<double>>(lfp_source, Reduction::Mean));

    // 2. Add the transformed column that depends on "LFP_Mean".
    builder.addColumn<double>("LFP_Mean_ZScore",
        std::make_unique<StandardizeComputer<double>>("LFP_Mean"));

    TableView table = builder.build();

    // Retrieve the standardized data.
    // This will first trigger the computation of "LFP_Mean", then use those
    // results to compute "LFP_Mean_ZScore".
    const auto& z_scores = table.getColumnValues<double>("LFP_Mean_ZScore");

    if (!z_scores.empty()) {
        std::cout << "First Z-scored value: " << z_scores[0] << std::endl;
    }
}

Addendum 4: Support for Reverse Lookup (Trace-back to Source)
Date: July 15, 2025

This addendum addresses the critical requirement for interactive plotting: tracing a data point in a TableView row back to its original source definition (e.g., the specific timestamp or interval).

A10. Problem Statement
The TableView contains all the information necessary for a reverse lookup, but it is not exposed through a clean, type-safe public API. Client code, such as a plotting library, should not need to know the internal details of the IRowSelector or perform dynamic_cast operations to get the source of a given row.

A11. Proposed Design Changes
We will introduce a RowDescriptor variant type and a new public method on TableView to provide this functionality in a clean, encapsulated way.

1. RowDescriptor (New Type Definition):
A std::variant that can hold any of the possible source types that can define a row. This makes it easily extensible.

// Assuming Interval and TimeFrameIndex are defined elsewhere
using RowDescriptor = std::variant<
    std::monostate,         // For cases where there's no descriptor
    size_t,                 // For IndexSelector
    double,                 // For TimestampSelector
    Interval                // For IntervalSelector
>;

2. IRowSelector Extension:
The IRowSelector interface will be extended with a virtual method to retrieve a descriptor for a specific row.

class IRowSelector {
public:
    // ... existing methods ...
    virtual RowDescriptor getDescriptor(size_t row_index) const = 0;
};

// Example implementation in IntervalSelector:
RowDescriptor IntervalSelector::getDescriptor(size_t row_index) const {
    if (row_index < m_intervals.size()) {
        return m_intervals[row_index];
    }
    return std::monostate{};
}

3. TableView API Extension:
A new public method is added to TableView to provide the main access point for this feature.

class TableView {
public:
    // ... existing methods ...
    
    // Gets a descriptor containing the source information for a given row index.
    RowDescriptor getRowDescriptor(size_t row_index) const;
};

// Implementation:
RowDescriptor TableView::getRowDescriptor(size_t row_index) const {
    if (m_rowSelector) {
        return m_rowSelector->getDescriptor(row_index);
    }
    return std::monostate{};
}

A12. Updated Usage Example (Tooltip Scenario)
This example shows how a plotting library would use the new API to create a tooltip.

// This function would be called by the plotting library on mouse hover.
void on_plot_hover(const TableView& table, size_t hovered_row_index) {
    // 1. Get the generic descriptor for the hovered row.
    RowDescriptor desc = table.getRowDescriptor(hovered_row_index);

    // 2. Use std::visit with a lambda to process the descriptor type-safely.
    std::string tooltip_text = std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, size_t>) {
            return "Source Index: " + std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            return "Source Timestamp: " + std::to_string(arg);
        } else if constexpr (std::is_same_v<T, Interval>) {
            return "Source Interval: [" + std::to_string(arg.start) + ", " + std::to_string(arg.end) + "]";
        } else {
            return "Source: N/A";
        }
    }, desc);

    // 3. Display the generated tooltip text to the user.
    show_tooltip(tooltip_text);
}


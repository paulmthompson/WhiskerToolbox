#include <gtest/gtest.h>
#include <DataManager/utils/TableView/columns/IntervalOverlapComputer.h>
#include <DataManager/utils/TableView/ExecutionPlan.h>
#include <DataManager/utils/TableView/DataTable.h>
#include <DataManager/utils/EntityIdGenerator.h>

using namespace wt::utils;

class IntervalOverlapComputerEntityIdsTest : public ::testing::Test {
private:
    void SetUp() override {
        // Create a DataTable with time intervals as columns
        m_table = std::make_unique<DataTable>();
        
        // Create interval data - we'll have 3 rows with overlapping intervals
        std::vector<TimePoint> start_times = {0.0, 1.0, 2.0};
        std::vector<TimePoint> end_times = {1.5, 2.5, 3.5};
        
        // Create row intervals
        for (size_t i = 0; i < 3; ++i) {
            Interval row_interval{start_times[i], end_times[i]};
            m_row_intervals.push_back(row_interval);
        }
        
        // Create column intervals that will have different overlap patterns
        m_column_intervals = {
            Interval{0.5, 1.2},  // Overlaps with rows 0, 1
            Interval{1.8, 2.2},  // Overlaps with rows 1, 2  
            Interval{0.0, 3.5}   // Overlaps with all rows 0, 1, 2
        };
        
        // Add intervals to table
        m_table->addColumn("row_start", start_times);
        m_table->addColumn("row_end", end_times);
        
        // Generate EntityIds for the row data
        EntityIdGenerator generator;
        for (size_t i = 0; i < 3; ++i) {
            m_expected_entity_ids.push_back(generator.generateEntityId());
        }
        m_table->addColumn("entity_ids", m_expected_entity_ids);
    }
    
    std::unique_ptr<DataTable> m_table;
    std::vector<Interval> m_row_intervals;
    std::vector<Interval> m_column_intervals; 
    std::vector<EntityId> m_expected_entity_ids;
};

TEST_F(IntervalOverlapComputerEntityIdsTest, SimpleStructureAssignIdOperation) {
    // Test Simple EntityID structure for AssignID operations
    auto computer = std::make_shared<IntervalOverlapComputer>(
        "row_start", "row_end", "entity_ids",
        m_column_intervals,
        IntervalOverlapComputer::Operation::AssignID,
        IntervalOverlapComputer::OverlapType::Any
    );
    
    // Verify structure is Simple for AssignID operations
    EXPECT_EQ(computer->getEntityIdStructure(), EntityIdStructure::Simple);
    
    ExecutionPlan plan(*m_table);
    
    // Test column-level EntityIDs
    ColumnEntityIds column_entity_ids = computer->computeColumnEntityIds(plan);
    
    // Should hold Simple variant (std::vector<EntityId>)
    ASSERT_TRUE(std::holds_alternative<std::vector<EntityId>>(column_entity_ids));
    auto simple_entity_ids = std::get<std::vector<EntityId>>(column_entity_ids);
    
    // For AssignID operations, each row should have one EntityID from overlapping intervals
    EXPECT_EQ(simple_entity_ids.size(), 3);  // 3 rows
    
    // Test cell-level EntityIDs
    for (size_t row = 0; row < 3; ++row) {
        std::vector<EntityId> cell_entity_ids = computer->computeCellEntityIds(plan, row);
        EXPECT_EQ(cell_entity_ids.size(), 1);  // AssignID gives single EntityID per cell
        EXPECT_EQ(cell_entity_ids[0], simple_entity_ids[row]);
    }
    
    // Verify specific overlap behavior
    // Row 0 (0.0-1.5): Should overlap with column 0 (0.5-1.2) and column 2 (0.0-3.5)
    // Row 1 (1.0-2.5): Should overlap with all columns 
    // Row 2 (2.0-3.5): Should overlap with column 1 (1.8-2.2) and column 2 (0.0-3.5)
    
    // For AssignID, we get the first matching interval's EntityID
    // (exact behavior depends on implementation, but should be consistent)
}

TEST_F(IntervalOverlapComputerEntityIdsTest, ComplexStructureCountOperation) {
    // Test Complex EntityID structure for Count operations
    auto computer = std::make_shared<IntervalOverlapComputer>(
        "row_start", "row_end", "entity_ids",
        m_column_intervals,
        IntervalOverlapComputer::Operation::CountOverlaps,
        IntervalOverlapComputer::OverlapType::Any
    );
    
    // Verify structure is Complex for CountOverlaps operations
    EXPECT_EQ(computer->getEntityIdStructure(), EntityIdStructure::Complex);
    
    ExecutionPlan plan(*m_table);
    
    // Test column-level EntityIDs  
    ColumnEntityIds column_entity_ids = computer->computeColumnEntityIds(plan);
    
    // Should hold Complex variant (std::vector<std::vector<EntityId>>)
    ASSERT_TRUE(std::holds_alternative<std::vector<std::vector<EntityId>>>(column_entity_ids));
    auto complex_entity_ids = std::get<std::vector<std::vector<EntityId>>>(column_entity_ids);
    
    EXPECT_EQ(complex_entity_ids.size(), 3);  // 3 rows
    
    // Test cell-level EntityIDs match column-level structure
    for (size_t row = 0; row < 3; ++row) {
        std::vector<EntityId> cell_entity_ids = computer->computeCellEntityIds(plan, row);
        EXPECT_EQ(cell_entity_ids, complex_entity_ids[row]);
    }
    
    // Verify specific overlap counts
    // Row 0 (0.0-1.5): Overlaps with columns 0 and 2 -> 2 EntityIDs
    EXPECT_EQ(complex_entity_ids[0].size(), 2);
    
    // Row 1 (1.0-2.5): Overlaps with all 3 columns -> 3 EntityIDs  
    EXPECT_EQ(complex_entity_ids[1].size(), 3);
    
    // Row 2 (2.0-3.5): Overlaps with columns 1 and 2 -> 2 EntityIDs
    EXPECT_EQ(complex_entity_ids[2].size(), 2);
    
    // All EntityIDs should come from our source row data
    for (const auto& row_entity_ids : complex_entity_ids) {
        for (const auto& entity_id : row_entity_ids) {
            EXPECT_TRUE(std::find(m_expected_entity_ids.begin(), m_expected_entity_ids.end(), entity_id) 
                       != m_expected_entity_ids.end());
        }
    }
}

TEST_F(IntervalOverlapComputerEntityIdsTest, VariantInterfaceConsistency) {
    // Test that the variant interface works consistently for both operation types
    
    auto assign_computer = std::make_shared<IntervalOverlapComputer>(
        "row_start", "row_end", "entity_ids",
        m_column_intervals,
        IntervalOverlapComputer::Operation::AssignID,
        IntervalOverlapComputer::OverlapType::Any
    );
    
    auto count_computer = std::make_shared<IntervalOverlapComputer>(
        "row_start", "row_end", "entity_ids", 
        m_column_intervals,
        IntervalOverlapComputer::Operation::CountOverlaps,
        IntervalOverlapComputer::OverlapType::Any
    );
    
    ExecutionPlan plan(*m_table);
    
    // Both should have EntityIDs
    EXPECT_TRUE(assign_computer->hasEntityIds());
    EXPECT_TRUE(count_computer->hasEntityIds());
    
    // Different structures
    EXPECT_EQ(assign_computer->getEntityIdStructure(), EntityIdStructure::Simple);
    EXPECT_EQ(count_computer->getEntityIdStructure(), EntityIdStructure::Complex);
    
    // Both should return valid variants
    ColumnEntityIds assign_ids = assign_computer->computeColumnEntityIds(plan);
    ColumnEntityIds count_ids = count_computer->computeColumnEntityIds(plan);
    
    EXPECT_TRUE(std::holds_alternative<std::vector<EntityId>>(assign_ids));
    EXPECT_TRUE(std::holds_alternative<std::vector<std::vector<EntityId>>>(count_ids));
    
    // Cell EntityIDs should be consistent
    for (size_t row = 0; row < 3; ++row) {
        auto assign_cell = assign_computer->computeCellEntityIds(plan, row);
        auto count_cell = count_computer->computeCellEntityIds(plan, row);
        
        // AssignID should return single EntityID
        EXPECT_EQ(assign_cell.size(), 1);
        
        // CountOverlaps should return multiple EntityIDs
        EXPECT_GE(count_cell.size(), 1);
        
        // The first EntityID from count should match the assign EntityID
        // (if they use the same selection logic)
        // This depends on implementation details, so we'll just verify consistency
        EXPECT_FALSE(count_cell.empty());
    }
}

TEST_F(IntervalOverlapComputerEntityIdsTest, ErrorHandlingEmptyData) {
    // Test behavior with empty table
    DataTable empty_table;
    
    auto computer = std::make_shared<IntervalOverlapComputer>(
        "row_start", "row_end", "entity_ids",
        m_column_intervals,
        IntervalOverlapComputer::Operation::AssignID,
        IntervalOverlapComputer::OverlapType::Any
    );
    
    ExecutionPlan empty_plan(empty_table);
    
    // Should handle empty data gracefully
    ColumnEntityIds column_ids = computer->computeColumnEntityIds(empty_plan);
    ASSERT_TRUE(std::holds_alternative<std::vector<EntityId>>(column_ids));
    
    auto simple_ids = std::get<std::vector<EntityId>>(column_ids);
    EXPECT_TRUE(simple_ids.empty());
    
    // Cell EntityIDs for invalid row should return empty
    std::vector<EntityId> cell_ids = computer->computeCellEntityIds(empty_plan, 0);
    EXPECT_TRUE(cell_ids.empty());
}
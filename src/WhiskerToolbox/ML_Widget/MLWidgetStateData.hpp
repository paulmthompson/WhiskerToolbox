#ifndef ML_WIDGET_STATE_DATA_HPP
#define ML_WIDGET_STATE_DATA_HPP

/**
 * @file MLWidgetStateData.hpp
 * @brief Serializable state data structure for ML_Widget
 * 
 * This file defines the state structure that MLWidgetState serializes to JSON
 * using reflect-cpp. It tracks machine learning configuration and model state.
 * 
 * ## State Tracked
 * 
 * - Selected table for training data
 * - Selected feature columns
 * - Selected label column
 * - Selected mask columns
 * - Training interval key
 * - Selected model type
 * - Selected outcomes
 * 
 * @see MLWidgetState for the Qt wrapper class
 * @see ML_Widget for the widget implementation
 */

#include <rfl.hpp>

#include <string>
#include <vector>

/**
 * @brief Serializable state data for ML_Widget
 * 
 * All members are designed for reflect-cpp serialization.
 * No Qt types are used to ensure clean JSON output.
 */
struct MLWidgetStateData {
    // === Metadata ===
    std::string instance_id;                         ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Machine Learning";   ///< User-visible name
    
    // === Table-based ML State ===
    std::string selected_table_id;                   ///< Selected table for training data
    std::vector<std::string> selected_feature_columns;  ///< Feature columns for training
    std::vector<std::string> selected_mask_columns;     ///< Mask columns for filtering
    std::string selected_label_column;               ///< Label column for classification
    
    // === Training Configuration ===
    std::string training_interval_key;               ///< Key of the interval series for training
    std::string selected_model_type;                 ///< Currently selected model type (e.g., "Random Forest")
    
    // === Selected Outcomes ===
    std::vector<std::string> selected_outcomes;      ///< Selected outcome features
};

#endif // ML_WIDGET_STATE_DATA_HPP

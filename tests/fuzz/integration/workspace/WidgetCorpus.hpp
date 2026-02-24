/**
 * @file WidgetCorpus.hpp
 * @brief Graduated real widget type inclusion for workspace fuzz testing
 *
 * Defines corpus levels that progressively include more real widget types
 * in fuzz testing, replacing mock editor types with production code.
 *
 * Phase 6 of the Workspace Fuzz Testing Roadmap.
 *
 * @see WORKSPACE_FUZZ_TESTING_ROADMAP.md for adoption order and rationale
 */

#ifndef FUZZ_WIDGET_CORPUS_HPP
#define FUZZ_WIDGET_CORPUS_HPP

#include "EditorState/EditorRegistry.hpp"

#include <memory>
#include <string>
#include <vector>

class DataManager;

/**
 * @brief Corpus levels controlling which real widget types are included
 *
 * Each level includes all types from the previous level plus new ones.
 * Fuzz tests parameterize on the level so CI can run a fast subset
 * while local fuzzing can exercise the full set.
 */
enum class CorpusLevel {
    Mock,       ///< Phase 3-4 mock types only (no real widgets)
    Minimal,    ///< TestWidget only (first real widget, simplest)
    // Future levels:
    // Core,    ///< + MediaWidget, DataViewer, DataTransform
    // Plotting,///< + LinePlot, EventPlot, ScatterPlot, Heatmap, PSTH, ACF
    // Full     ///< All editor types
};

/**
 * @brief Descriptor for a widget type in the corpus
 *
 * Contains the type_id used for lookup and creation, plus metadata
 * useful for fuzz test logic (e.g., whether duplicates are allowed).
 */
struct CorpusEntry {
    std::string type_id;         ///< EditorRegistry type ID (e.g., "TestWidget")
    bool allow_multiple = false; ///< Whether multiple instances are permitted
};

/**
 * @brief Register real widget types with an EditorRegistry at a given corpus level
 *
 * This function registers production EditorState types (with real state factories
 * and minimal QWidget view/properties factories) so that fuzz tests exercise real
 * serialization code paths.
 *
 * View and properties factories produce minimal QWidget instances (not the real
 * UI classes) to avoid heavy dependencies while still exercising ADS docking.
 *
 * @param registry The EditorRegistry to register types with
 * @param level Which corpus level of types to register
 * @param data_manager Shared DataManager for widget state constructors that need it
 */
void registerCorpusTypes(
    EditorRegistry * registry,
    CorpusLevel level,
    std::shared_ptr<DataManager> data_manager);

/**
 * @brief Get the list of corpus entries at a given level
 *
 * Returns the type descriptors for all types included at and below the
 * specified level. Useful for fuzz test domain generation.
 *
 * @param level Corpus level
 * @return Vector of CorpusEntry descriptors
 */
std::vector<CorpusEntry> corpusEntries(CorpusLevel level);

#endif // FUZZ_WIDGET_CORPUS_HPP

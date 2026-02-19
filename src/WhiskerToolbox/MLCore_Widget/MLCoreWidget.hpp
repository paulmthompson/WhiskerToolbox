#ifndef MLCORE_WIDGET_HPP
#define MLCORE_WIDGET_HPP

/**
 * @file MLCoreWidget.hpp
 * @brief Main widget for the MLCore ML workflow UI
 *
 * MLCoreWidget is the top-level view widget for interactive machine learning
 * workflows in WhiskerToolbox. It provides a tab-based interface for:
 *
 * - **Classification**: Supervised classification using MLCore pipelines
 * - **Clustering**: Unsupervised clustering using MLCore pipelines
 *
 * This is a stub implementation for task 4.1 (state + registration).
 * Sub-panels (FeatureSelectionPanel, ModelConfigPanel, etc.) will be
 * added in subsequent tasks (4.2–4.7).
 *
 * ## Architecture
 *
 * MLCoreWidget follows the self-contained tool widget pattern:
 * - Single widget (no view/properties split)
 * - Placed in Zone::Right as a tabbed panel
 * - Consumes MLCoreWidgetState for serializable configuration
 * - Accesses DataManager for data keys and SelectionContext for focus awareness
 *
 * @see MLCoreWidgetState for the state this widget observes
 * @see MLCoreWidgetRegistration for how this widget is created
 * @see ClassificationPipeline for the underlying supervised pipeline
 * @see ClusteringPipeline for the underlying unsupervised pipeline
 */

#include <QWidget>

#include <memory>

// Forward declarations
class DataManager;
class MLCoreWidgetState;
class SelectionContext;

class MLCoreWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the ML workflow widget
     *
     * @param state Shared state object for serialization and signal coordination
     * @param data_manager Shared DataManager for accessing data keys
     * @param selection_context SelectionContext for passive data focus awareness
     * @param parent Optional parent widget
     */
    explicit MLCoreWidget(std::shared_ptr<MLCoreWidgetState> state,
                          std::shared_ptr<DataManager> data_manager,
                          SelectionContext * selection_context,
                          QWidget * parent = nullptr);

    ~MLCoreWidget() override = default;

private:
    void _setupUi();
    void _connectSignals();

    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    SelectionContext * _selection_context;
};

#endif // MLCORE_WIDGET_HPP

/// @file PostEncoderWidget.hpp
/// @brief Self-contained widget for configuring the post-encoder module section.
///
/// Uses PostEncoderModuleRegistry for module discovery and schema-driven
/// parameter editing via AutoParamWidget.

#ifndef POST_ENCODER_WIDGET_HPP
#define POST_ENCODER_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <QWidget>

#include <memory>
#include <string>

class AutoParamWidget;
class DataManager;
class QLabel;
class QComboBox;
class QVBoxLayout;
class DeepLearningState;
class SlotAssembler;
struct ImageSize;

namespace dl::widget {

/**
 * @brief Widget for configuring the post-encoder module.
 *
 * Presents a module combo populated from `PostEncoderModuleRegistry` and a
 * dynamic parameter form loaded from the registry schema for the selected module.
 * The point_key combo (for spatial_point) is populated from DataManager.
 *
 * @note Not thread-safe — must be used from the GUI thread only.
 */
class PostEncoderWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the post-encoder widget.
     * @param state     DeepLearningState to sync post-encoder params.
     * @param dm        Shared DataManager for populating the point_key combo.
     * @param assembler SlotAssembler for configurePostEncoderModule.
     * @param parent    Optional parent widget.
     *
     * @pre state, dm, and assembler must not be null.
     */
    explicit PostEncoderWidget(
            std::shared_ptr<DeepLearningState> state,
            std::shared_ptr<DataManager> dm,
            SlotAssembler * assembler,
            QWidget * parent = nullptr);

    ~PostEncoderWidget() override;

    // Non-copyable, non-movable (QWidget)
    PostEncoderWidget(PostEncoderWidget const &) = delete;
    PostEncoderWidget & operator=(PostEncoderWidget const &) = delete;
    PostEncoderWidget(PostEncoderWidget &&) = delete;
    PostEncoderWidget & operator=(PostEncoderWidget &&) = delete;

    /// @brief Return the current parameter values.
    [[nodiscard]] PostEncoderSlotParams params() const;

    /// @brief Set the parameter values and update the UI.
    void setParams(PostEncoderSlotParams const & params);

    /// @brief Refresh the point_key combo from DataManager.
    void refreshDataSources();

signals:
    /// Emitted whenever any parameter in the post-encoder section changes.
    void bindingChanged();

private:
    /// Apply current params to state and assembler.
    void _applyToStateAndAssembler();

    /// Get source image size from primary media binding.
    [[nodiscard]] ImageSize _sourceImageSize() const;

    /// Populate the module combo from the registry.
    void _populateModuleCombo();

    /// Read the currently selected module key from the combo.
    [[nodiscard]] std::string _selectedModuleKey() const;

    /// Rebuild the parameter panel for the current module selection.
    void _rebuildParamsPanel(std::string const & params_json);

    /// Clear and destroy the current parameter widget.
    void _clearParamsPanel();

    std::shared_ptr<DeepLearningState> _state;
    std::shared_ptr<DataManager> _dm;
    SlotAssembler * _assembler;

    QComboBox * _module_combo = nullptr;
    QLabel * _description_label = nullptr;
    QVBoxLayout * _params_layout = nullptr;
    AutoParamWidget * _auto_param = nullptr;
    bool _suppress_signals = false;
};

}// namespace dl::widget

#endif// POST_ENCODER_WIDGET_HPP

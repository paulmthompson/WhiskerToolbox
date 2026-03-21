/// @file PostEncoderWidget.hpp
/// @brief Self-contained widget for configuring the post-encoder module section.
///
/// Replaces the hand-built `_buildPostEncoderSection()` panel with a
/// schema-driven form powered by AutoParamWidget. The widget owns a
/// `PostEncoderSlotParams` struct and applies changes to DeepLearningState
/// and SlotAssembler.

#ifndef POST_ENCODER_WIDGET_HPP
#define POST_ENCODER_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <QWidget>

#include <memory>
#include <string>

class AutoParamWidget;
class DataManager;
class QGroupBox;
class DeepLearningState;
class SlotAssembler;
struct ImageSize;

namespace dl::widget {

/**
 * @brief Widget for configuring the post-encoder module.
 *
 * Wraps an AutoParamWidget driven by the `PostEncoderSlotParams` schema.
 * The module variant offers None, GlobalAvgPool, and SpatialPoint options.
 * The point_key combo (for SpatialPoint) is populated from DataManager.
 *
 * @note Not thread-safe — must be used from the GUI thread only.
 */
class PostEncoderWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the post-encoder widget.
     * @param state     DeepLearningState to sync module type and point key.
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

    /// @brief Return the module type string for decoder consistency enforcement.
    /// Returns "none", "global_avg_pool", or "spatial_point".
    [[nodiscard]] std::string moduleTypeForState() const;

    /// @brief Build PostEncoderSlotParams from saved state (for restore).
    [[nodiscard]] static PostEncoderSlotParams paramsFromState(
            std::string const & module_type,
            std::string const & point_key);

signals:
    /// Emitted whenever any parameter in the post-encoder section changes.
    void bindingChanged();

private:
    /// Apply current params to state and assembler.
    void _applyToStateAndAssembler();

    /// Get source image size from primary media binding.
    [[nodiscard]] ImageSize _sourceImageSize() const;

    std::shared_ptr<DeepLearningState> _state;
    std::shared_ptr<DataManager> _dm;
    SlotAssembler * _assembler;

    AutoParamWidget * _auto_param = nullptr;
};

}// namespace dl::widget

#endif// POST_ENCODER_WIDGET_HPP

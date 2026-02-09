#ifndef ONION_SKIN_VIEW_STATE_HPP
#define ONION_SKIN_VIEW_STATE_HPP

/**
 * @file OnionSkinViewState.hpp
 * @brief State class for OnionSkinViewWidget
 *
 * OnionSkinViewState manages the serializable state for the
 * OnionSkinViewWidget. It includes temporal window parameters (behind/ahead),
 * alpha curve settings, data keys for point/line/mask data, and rendering
 * parameters. View state (CorePlotting::ViewStateData) is the single source
 * of truth for zoom, pan, and data bounds.
 *
 * @see EditorState for base class documentation
 * @see TemporalProjectionViewState for the full-overlay counterpart
 */

#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "EditorState/EditorState.hpp"
#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisStateData.hpp"
#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisStateData.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Serializable state data for OnionSkinViewWidget
 */
struct OnionSkinViewStateData {
    std::string instance_id;
    std::string display_name = "Onion Skin View";
    CorePlotting::ViewStateData view_state;
    HorizontalAxisStateData horizontal_axis;
    VerticalAxisStateData vertical_axis;

    // Data keys
    std::vector<std::string> point_data_keys;
    std::vector<std::string> line_data_keys;
    std::vector<std::string> mask_data_keys;

    // Temporal window
    int window_behind = 5;   ///< Samples before current time
    int window_ahead = 5;    ///< Samples after current time

    // Alpha curve
    std::string alpha_curve = "linear"; ///< "linear", "exponential", "gaussian"
    float min_alpha = 0.1f;
    float max_alpha = 1.0f;

    // Rendering
    float point_size = 8.0f;
    float line_width = 2.0f;
    bool highlight_current = true;  ///< Draw current frame with distinct color/size
};

/**
 * @brief State class for OnionSkinViewWidget
 *
 * Single source of truth: view_state (zoom/pan) plus horizontal and vertical
 * axis states (full range), temporal window parameters, alpha curve settings,
 * data keys, and rendering parameters.
 */
class OnionSkinViewState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new OnionSkinViewState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit OnionSkinViewState(QObject * parent = nullptr);

    ~OnionSkinViewState() override = default;

    // === Type Identification ===

    [[nodiscard]] QString getTypeName() const override
    {
        return QStringLiteral("OnionSkinView");
    }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Axis state access (for widgets and serialization) ===
    [[nodiscard]] HorizontalAxisState * horizontalAxisState()
    {
        return _horizontal_axis_state.get();
    }
    [[nodiscard]] VerticalAxisState * verticalAxisState()
    {
        return _vertical_axis_state.get();
    }

    // === View state (zoom / pan / bounds) ===
    /** @brief Get the current view state (bounds + zoom + pan). */
    [[nodiscard]] CorePlotting::ViewStateData const & viewState() const
    {
        return _data.view_state;
    }
    void setXZoom(double zoom);
    void setYZoom(double zoom);
    void setPan(double x_pan, double y_pan);

    /**
     * @brief Set X data bounds and keep horizontal axis in sync.
     * @param x_min Minimum X value
     * @param x_max Maximum X value
     */
    void setXBounds(double x_min, double x_max);

    /**
     * @brief Set Y data bounds and keep vertical axis in sync.
     * @param y_min Minimum Y value
     * @param y_max Maximum Y value
     */
    void setYBounds(double y_min, double y_max);

    // === Data Key Management ===

    /** @brief Get all point data keys */
    [[nodiscard]] std::vector<QString> getPointDataKeys() const;
    void addPointDataKey(QString const & key);
    void removePointDataKey(QString const & key);
    void clearPointDataKeys();

    /** @brief Get all line data keys */
    [[nodiscard]] std::vector<QString> getLineDataKeys() const;
    void addLineDataKey(QString const & key);
    void removeLineDataKey(QString const & key);
    void clearLineDataKeys();

    /** @brief Get all mask data keys */
    [[nodiscard]] std::vector<QString> getMaskDataKeys() const;
    void addMaskDataKey(QString const & key);
    void removeMaskDataKey(QString const & key);
    void clearMaskDataKeys();

    // === Temporal Window Parameters ===

    [[nodiscard]] int getWindowBehind() const { return _data.window_behind; }
    void setWindowBehind(int behind);

    [[nodiscard]] int getWindowAhead() const { return _data.window_ahead; }
    void setWindowAhead(int ahead);

    // === Alpha Curve Settings ===

    [[nodiscard]] QString getAlphaCurve() const { return QString::fromStdString(_data.alpha_curve); }
    void setAlphaCurve(QString const & curve);

    [[nodiscard]] float getMinAlpha() const { return _data.min_alpha; }
    void setMinAlpha(float alpha);

    [[nodiscard]] float getMaxAlpha() const { return _data.max_alpha; }
    void setMaxAlpha(float alpha);

    // === Rendering Parameters ===

    [[nodiscard]] float getPointSize() const { return _data.point_size; }
    void setPointSize(float size);

    [[nodiscard]] float getLineWidth() const { return _data.line_width; }
    void setLineWidth(float width);

    [[nodiscard]] bool getHighlightCurrent() const { return _data.highlight_current; }
    void setHighlightCurrent(bool highlight);

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

signals:
    void viewStateChanged();

    // Data key signals
    void pointDataKeyAdded(QString const & key);
    void pointDataKeyRemoved(QString const & key);
    void pointDataKeysCleared();
    void lineDataKeyAdded(QString const & key);
    void lineDataKeyRemoved(QString const & key);
    void lineDataKeysCleared();
    void maskDataKeyAdded(QString const & key);
    void maskDataKeyRemoved(QString const & key);
    void maskDataKeysCleared();

    // Temporal window signals
    void windowBehindChanged(int behind);
    void windowAheadChanged(int ahead);

    // Alpha curve signals
    void alphaCurveChanged(QString const & curve);
    void minAlphaChanged(float alpha);
    void maxAlphaChanged(float alpha);

    // Rendering signals
    void pointSizeChanged(float size);
    void lineWidthChanged(float width);
    void highlightCurrentChanged(bool highlight);

private:
    OnionSkinViewStateData _data;
    std::unique_ptr<HorizontalAxisState> _horizontal_axis_state;
    std::unique_ptr<VerticalAxisState> _vertical_axis_state;
};

#endif  // ONION_SKIN_VIEW_STATE_HPP

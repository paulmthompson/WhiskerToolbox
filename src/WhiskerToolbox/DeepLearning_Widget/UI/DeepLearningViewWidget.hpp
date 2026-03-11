#ifndef DEEP_LEARNING_VIEW_WIDGET_HPP
#define DEEP_LEARNING_VIEW_WIDGET_HPP

/**
 * @file DeepLearningViewWidget.hpp
 * @brief View panel for deep learning inference results.
 *
 * Displays model status, cached static tensor previews (shape, range),
 * output tensor overlays, and a progress bar during batch inference.
 */

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class QLabel;
class QProgressBar;
class QVBoxLayout;

class DataManager;
class DeepLearningState;
class SlotAssembler;

class DeepLearningViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit DeepLearningViewWidget(
            std::shared_ptr<DeepLearningState> state,
            std::shared_ptr<DataManager> data_manager,
            QWidget * parent = nullptr);

    ~DeepLearningViewWidget() override;

    /// Set the SlotAssembler reference for cache preview queries.
    /// Called by the registration module after both widgets are created.
    void setAssembler(SlotAssembler * assembler);

public slots:
    /// Refresh the static tensor cache preview display.
    void refreshCachePreview();

    /// Update the progress bar during recurrent inference.
    /// @param current 0-based frame being processed
    /// @param total Total number of frames to process
    void updateRecurrentProgress(int current, int total);

private:
    void _buildUi();
    void _onModelChanged();
    void _clearPreviewArea();

    std::shared_ptr<DeepLearningState> _state;
    std::shared_ptr<DataManager> _data_manager;

    /// Non-owning pointer to the assembler (owned by PropertiesWidget).
    SlotAssembler * _assembler = nullptr;

    QLabel * _status_label = nullptr;
    QLabel * _model_info_label = nullptr;
    QWidget * _preview_area = nullptr;
    QVBoxLayout * _preview_layout = nullptr;
    QProgressBar * _progress_bar = nullptr;
};

#endif// DEEP_LEARNING_VIEW_WIDGET_HPP

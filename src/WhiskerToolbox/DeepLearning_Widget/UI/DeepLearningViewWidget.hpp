#ifndef DEEP_LEARNING_VIEW_WIDGET_HPP
#define DEEP_LEARNING_VIEW_WIDGET_HPP

/**
 * @file DeepLearningViewWidget.hpp
 * @brief View panel for deep learning inference results.
 *
 * Displays model status, input tensor channel previews (color-mapped),
 * output tensor overlays, and a progress bar during batch inference.
 */

#include <QWidget>

#include <memory>

class QLabel;
class QProgressBar;
class QVBoxLayout;

class DataManager;
class DeepLearningState;

class DeepLearningViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit DeepLearningViewWidget(
        std::shared_ptr<DeepLearningState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~DeepLearningViewWidget() override;

private:
    void _buildUi();
    void _onModelChanged();

    std::shared_ptr<DeepLearningState> _state;
    std::shared_ptr<DataManager> _data_manager;

    QLabel * _status_label = nullptr;
    QLabel * _model_info_label = nullptr;
    QWidget * _preview_area = nullptr;
    QVBoxLayout * _preview_layout = nullptr;
    QProgressBar * _progress_bar = nullptr;
};

#endif // DEEP_LEARNING_VIEW_WIDGET_HPP

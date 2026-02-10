#ifndef DEEP_LEARNING_VIEW_WIDGET_HPP
#define DEEP_LEARNING_VIEW_WIDGET_HPP

/**
 * @file DeepLearningViewWidget.hpp
 * @brief View panel for DeepLearning widget.
 *
 * DeepLearningViewWidget is the primary visualization panel associated
 * with DeepLearningState. It is intended to be placed in the center
 * zone, while a complementary properties panel lives in the right zone.
 *
 * The current implementation is a minimal Qt Designer-based widget
 * that can be extended with actual deep learning visualization logic.
 */

#include <QWidget>

#include <memory>

class DataManager;
class DeepLearningState;

namespace Ui {
class DeepLearningViewWidget;
}

/**
 * @brief View panel for DeepLearning widget.
 *
 * This widget will eventually display deep learning results (e.g.,
 * segmentation overlays, classification outputs) driven by
 * DeepLearningState and DataManager.
 */
class DeepLearningViewWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a new DeepLearningViewWidget.
     *
     * @param state Shared pointer to the DeepLearningState.
     * @param data_manager Shared DataManager for data access.
     * @param parent Parent widget.
     */
    explicit DeepLearningViewWidget(std::shared_ptr<DeepLearningState> state,
                                    std::shared_ptr<DataManager> data_manager,
                                    QWidget * parent = nullptr);

    /**
     * @brief Virtual destructor.
     */
    ~DeepLearningViewWidget() override;

private:
    Ui::DeepLearningViewWidget * ui;
    std::shared_ptr<DeepLearningState> _state;
    std::shared_ptr<DataManager> _data_manager;
};

#endif// DEEP_LEARNING_VIEW_WIDGET_HPP

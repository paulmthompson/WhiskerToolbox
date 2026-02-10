#ifndef DEEP_LEARNING_PROPERTIES_WIDGET_HPP
#define DEEP_LEARNING_PROPERTIES_WIDGET_HPP

/**
 * @file DeepLearningPropertiesWidget.hpp
 * @brief Properties panel for DeepLearning widget.
 *
 * DeepLearningPropertiesWidget is the companion properties panel for
 * DeepLearningViewWidget. It is intended to live in the properties
 * zone (typically the right dock area) and will eventually expose
 * configuration controls for deep learning operations.
 */

#include <QWidget>

#include <memory>

class DataManager;
class DeepLearningState;

namespace Ui {
class DeepLearningPropertiesWidget;
}

/**
 * @brief Properties panel for DeepLearning widget.
 *
 * The current implementation is a minimal Qt Designer-based widget
 * that can be extended with actual configuration controls.
 */
class DeepLearningPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a new DeepLearningPropertiesWidget.
     *
     * @param state Shared pointer to the DeepLearningState.
     * @param data_manager Shared DataManager for data access.
     * @param parent Parent widget.
     */
    explicit DeepLearningPropertiesWidget(std::shared_ptr<DeepLearningState> state,
                                          std::shared_ptr<DataManager> data_manager,
                                          QWidget * parent = nullptr);

    /**
     * @brief Virtual destructor.
     */
    ~DeepLearningPropertiesWidget() override;

private:
    Ui::DeepLearningPropertiesWidget * ui;
    std::shared_ptr<DeepLearningState> _state;
    std::shared_ptr<DataManager> _data_manager;
};

#endif// DEEP_LEARNING_PROPERTIES_WIDGET_HPP

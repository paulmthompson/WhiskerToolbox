#ifndef SPECTROGRAM_WIDGET_HPP
#define SPECTROGRAM_WIDGET_HPP

/**
 * @file SpectrogramWidget.hpp
 * @brief Main widget for displaying spectrograms
 * 
 * SpectrogramWidget displays spectrograms of analog signals showing
 * frequency content over time.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class SpectrogramState;

namespace Ui {
class SpectrogramWidget;
}

/**
 * @brief Main widget for spectrogram visualization
 */
class SpectrogramWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a SpectrogramWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    SpectrogramWidget(std::shared_ptr<DataManager> data_manager,
                      QWidget * parent = nullptr);

    ~SpectrogramWidget() override;

    /**
     * @brief Set the SpectrogramState for this widget
     * 
     * The state manages all serializable settings. This widget shares
     * the state with the properties widget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<SpectrogramState> state);

    /**
     * @brief Get the current SpectrogramState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<SpectrogramState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] SpectrogramState * state();

signals:
    /**
     * @brief Emitted when a time position is selected in the view
     * @param position TimePosition to navigate to
     */
    void timePositionSelected(TimePosition position);

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::SpectrogramWidget * ui;

    /// Serializable state shared with properties widget
    std::shared_ptr<SpectrogramState> _state;
};

#endif// SPECTROGRAM_WIDGET_HPP

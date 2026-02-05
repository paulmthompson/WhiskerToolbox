#ifndef SPLIT_BUTTON_HANDLER_HPP
#define SPLIT_BUTTON_HANDLER_HPP

/**
 * @file SplitButtonHandler.hpp
 * @brief Adds custom split buttons to ADS dock area title bars
 * 
 * SplitButtonHandler integrates with the Advanced Docking System to add
 * split buttons to dock area title bars, similar to VS Code's editor splitting.
 * 
 * ## Architecture
 * 
 * The handler connects to CDockManager::dockAreaCreated signal and adds
 * a split button to each new dock area's title bar. The button uses the
 * CDockAreaTitleBar::insertWidget() API.
 * 
 * ## Usage
 * 
 * ```cpp
 * // In MainWindow constructor, after creating dock manager
 * _split_button_handler = std::make_unique<SplitButtonHandler>(_m_DockManager, this);
 * 
 * // Connect to split request signal
 * connect(_split_button_handler.get(), &SplitButtonHandler::splitRequested,
 *         this, &MainWindow::handleSplitRequest);
 * ```
 * 
 * ## Customization
 * 
 * The split button appearance can be customized via Qt stylesheets:
 * ```css
 * QToolButton#SplitButton {
 *     background: transparent;
 *     border: none;
 *     padding: 2px;
 * }
 * QToolButton#SplitButton:hover {
 *     background: palette(mid);
 * }
 * ```
 * 
 * @see ZoneManager for zone-based widget placement
 * @see EditorCreationController for editor lifecycle management
 */

#include <QList>
#include <QObject>
#include <QPointer>
#include <QToolButton>

// Forward declarations
namespace ads {
class CDockManager;
class CDockAreaWidget;
class CDockWidget;
}// namespace ads

/**
 * @brief Handles adding split buttons to ADS dock area title bars
 * 
 * This class monitors dock area creation and adds a split button to each
 * title bar. When clicked, the split button emits a signal with the dock
 * area and direction information.
 */
class SplitButtonHandler : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Split direction options
     */
    enum class SplitDirection {
        Horizontal,///< Split side-by-side (left/right)
        Vertical   ///< Split above/below
    };
    Q_ENUM(SplitDirection)

    /**
     * @brief Construct a SplitButtonHandler
     * 
     * @param dock_manager The ADS dock manager to monitor
     * @param parent Parent QObject (typically MainWindow)
     */
    explicit SplitButtonHandler(ads::CDockManager * dock_manager,
                                QObject * parent = nullptr);

    ~SplitButtonHandler() override;

    /**
     * @brief Enable or disable split buttons globally
     * 
     * When disabled, existing buttons are hidden but not removed.
     * When re-enabled, buttons become visible again.
     * 
     * @param enabled Whether split buttons should be shown
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if split buttons are enabled
     */
    [[nodiscard]] bool isEnabled() const { return _enabled; }

    /**
     * @brief Set the default split direction
     * 
     * This direction is used when the split button is clicked without
     * any modifier keys.
     * 
     * @param direction Default split direction
     */
    void setDefaultSplitDirection(SplitDirection direction);

    /**
     * @brief Get the default split direction
     */
    [[nodiscard]] SplitDirection defaultSplitDirection() const { return _default_direction; }

    /**
     * @brief Set a custom icon for the split button
     * 
     * If not set, a default split icon is used.
     * 
     * @param icon Custom icon to use
     */
    void setSplitIcon(QIcon const & icon);

    /**
     * @brief Get the current split icon
     */
    [[nodiscard]] QIcon splitIcon() const { return _split_icon; }

    /**
     * @brief Set tooltip text for the split button
     * 
     * @param tooltip Tooltip to display on hover
     */
    void setTooltip(QString const & tooltip);

signals:
    /**
     * @brief Emitted when user requests a split
     * 
     * The slot handling this signal should:
     * 1. Get the current dock widget from the dock area
     * 2. Create a duplicate/new widget for the split
     * 3. Add it to the appropriate side of the original
     * 
     * @param dock_area The dock area where split was requested
     * @param direction The split direction (horizontal or vertical)
     */
    void splitRequested(ads::CDockAreaWidget * dock_area,
                        SplitButtonHandler::SplitDirection direction);

    /**
     * @brief Emitted when user requests to split a specific dock widget
     * 
     * This is an alternative signal that provides the current dock widget
     * directly, which is often more convenient for implementation.
     * 
     * @param dock_widget The currently active dock widget in the area
     * @param direction The split direction
     */
    void splitDockWidgetRequested(ads::CDockWidget * dock_widget,
                                  SplitButtonHandler::SplitDirection direction);

private slots:
    /**
     * @brief Handle new dock area creation
     * 
     * Adds a split button to the new dock area's title bar.
     */
    void onDockAreaCreated(ads::CDockAreaWidget * dock_area);

    /**
     * @brief Handle split button click
     */
    void onSplitButtonClicked();

    /**
     * @brief Handle vertical split button click
     */
    void onVerticalSplitButtonClicked();

private:
    ads::CDockManager * _dock_manager;
    bool _enabled = true;
    SplitDirection _default_direction = SplitDirection::Horizontal;
    QIcon _split_icon;
    QString _tooltip;

    /// Track buttons we've created for cleanup (using QList since QPointer doesn't have a hash)
    QList<QPointer<QToolButton>> _split_buttons;

    /// Track vertical split buttons separately
    QList<QPointer<QToolButton>> _vertical_split_buttons;

    /**
     * @brief Create the default split icon (horizontal split - side by side)
     */
    static QIcon createDefaultSplitIcon();

    /**
     * @brief Create the default vertical split icon (top/bottom)
     */
    static QIcon createDefaultVerticalSplitIcon();

    /**
     * @brief Add split buttons to a dock area's title bar
     */
    void addSplitButtonToArea(ads::CDockAreaWidget * dock_area);
};

#endif// SPLIT_BUTTON_HANDLER_HPP

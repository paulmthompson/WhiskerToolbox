#ifndef DATA_FOCUS_AWARE_HPP
#define DATA_FOCUS_AWARE_HPP

/**
 * @file DataFocusAware.hpp
 * @brief Mixin interface for widgets that respond to data focus changes
 * 
 * DataFocusAware provides a standardized interface for the "Passive Awareness"
 * pattern introduced in Phase 1 of the Editor State refactoring.
 * 
 * ## Design Philosophy
 * 
 * Widgets implementing this interface will:
 * - Connect to SelectionContext::dataFocusChanged
 * - Update their internal state regardless of visibility
 * - Provide consistent handling across all data-aware widgets
 * - NOT automatically raise themselves to the front
 * 
 * ## Usage Example
 * 
 * ```cpp
 * class MyWidget : public QWidget, public DataFocusAware {
 *     Q_OBJECT
 * public:
 *     MyWidget(SelectionContext * ctx, QWidget * parent = nullptr)
 *         : QWidget(parent), _selection_context(ctx)
 *     {
 *         connectToSelectionContext(ctx);
 *     }
 * 
 *     void onDataFocusChanged(SelectedDataKey const & data_key,
 *                              QString const & data_type) override
 *     {
 *         // Update internal model
 *         _current_data_key = data_key;
 *         
 *         // Refresh UI based on data type
 *         if (data_type == "LineData") {
 *             showLineProperties();
 *         } else if (data_type == "MaskData") {
 *             showMaskProperties();
 *         }
 *     }
 * };
 * ```
 * 
 * ## Thread Safety
 * 
 * The onDataFocusChanged() callback is always called from the main/GUI thread.
 * Implementations should not perform blocking operations in this callback.
 * 
 * @see SelectionContext for the data focus source
 * @see EditorState for widget state management
 */

#include "SelectionContext.hpp"
#include "StrongTypes.hpp"

#include <QObject>

/**
 * @brief Mixin interface for widgets that respond to data focus changes
 * 
 * This is a pure interface class (no data members, only virtual methods).
 * Implementations should:
 * 1. Call connectToSelectionContext() in constructor
 * 2. Implement onDataFocusChanged() to handle focus updates
 */
class DataFocusAware {
public:
    virtual ~DataFocusAware() = default;

    /**
     * @brief Called when data focus changes
     * 
     * Implementations should:
     * 1. Update internal model/state
     * 2. Refresh UI if currently visible
     * 3. NOT raise themselves to front (user controls tab visibility)
     * 
     * @param data_key The newly focused data key (may be invalid if nothing focused)
     * @param data_type The type of the focused data (e.g., "LineData", "MaskData")
     */
    virtual void onDataFocusChanged(EditorLib::SelectedDataKey const & data_key,
                                     QString const & data_type) = 0;

protected:
    /**
     * @brief Helper to connect this widget to SelectionContext
     * 
     * Call this in your widget's constructor after initializing
     * the SelectionContext pointer.
     * 
     * @param ctx The SelectionContext to observe
     * @param receiver The QObject that will receive the signal (typically 'this')
     * 
     * Example:
     * ```cpp
     * MyWidget::MyWidget(SelectionContext * ctx, QWidget * parent)
     *     : QWidget(parent), _ctx(ctx)
     * {
     *     connectToSelectionContext(ctx, this);
     * }
     * ```
     */
    template<typename Receiver>
    void connectToSelectionContext(SelectionContext * ctx, Receiver * receiver) {
        if (!ctx) {
            return;
        }
        QObject::connect(ctx, &SelectionContext::dataFocusChanged,
                         receiver, [this](EditorLib::SelectedDataKey const & key,
                                          QString const & type,
                                          SelectionSource const & /* source */) {
                             onDataFocusChanged(key, type);
                         });
    }
};

#endif  // DATA_FOCUS_AWARE_HPP

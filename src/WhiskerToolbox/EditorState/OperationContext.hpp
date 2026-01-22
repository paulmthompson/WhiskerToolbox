#ifndef OPERATION_CONTEXT_HPP
#define OPERATION_CONTEXT_HPP

/**
 * @file OperationContext.hpp
 * @brief Manages transient data flow connections between widgets
 *
 * OperationContext handles the pattern where one widget (consumer) requests
 * output from another widget (producer) without going through DataManager.
 *
 * ## Core Concept
 *
 * Normally, widgets like DataTransformWidget create new data in DataManager.
 * But sometimes a widget (like a plot) wants to receive that output directly
 * to configure a transform chain without creating persistent data.
 *
 * OperationContext manages these temporary "pipes":
 *
 * ```
 * Normal flow:
 *   DataTransformWidget → creates → DataManager["new_key"]
 *
 * Operation flow:
 *   LinePlot requests operation
 *   DataTransformWidget → delivers to → LinePlot
 *   Operation closes
 *   Back to normal flow
 * ```
 *
 * ## Design Assumptions
 *
 * - Only one pending operation per producer type at a time
 * - Operations auto-close on selection change by default
 * - Producer widgets may be singleton (like DataTransformWidget)
 *
 * ## Typical Flow
 *
 * 1. Consumer calls requestOperation() → producer widget opens/focuses
 * 2. Producer checks pendingOperationFor() in its "apply" handler
 * 3. Producer calls deliverResult() → consumer receives via signal
 * 4. Operation closes (automatically or explicitly)
 *
 * @see EditorRegistry for widget lifecycle
 * @see SelectionContext for data selection (separate concern)
 * @see OperationResult for result payload
 */

#include "OperationResult.hpp"
#include "StrongTypes.hpp"

#include <QObject>

#include <map>
#include <optional>

// Forward declarations (global namespace)
class EditorRegistry;
class SelectionContext;

namespace EditorLib {

/**
 * @brief Reason an operation was closed
 */
enum class OperationCloseReason {
    Explicit,          ///< User/code explicitly closed
    SelectionChanged,  ///< User selected different data elsewhere
    RequesterClosed,   ///< The requesting widget was closed
    ProducerClosed,    ///< The producing widget was closed
    Delivered,         ///< Result was delivered (for one-shot operations)
    Superseded         ///< A new operation replaced this one
};

/**
 * @brief A pending request for data from one widget to another
 */
struct PendingOperation {
    OperationId id;               ///< Unique identifier for this operation
    EditorInstanceId requester;   ///< Who wants the result
    EditorTypeId producer_type;   ///< What type of widget produces it
    DataChannel channel;          ///< What kind of output expected

    /// If true, operation closes when user selects data elsewhere
    bool close_on_selection_change = true;

    /// If true, operation closes after first delivery
    bool close_after_delivery = false;
};

/**
 * @brief Options for requesting an operation
 */
struct OperationRequestOptions {
    DataChannel channel = DataChannels::TransformPipeline;
    bool close_on_selection_change = true;
    bool close_after_delivery = false;
};

/**
 * @brief Manages transient data flow connections between widgets
 */
class OperationContext : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct an OperationContext
     * @param registry The EditorRegistry (for widget lookup, may be nullptr for tests)
     * @param parent Parent QObject
     */
    explicit OperationContext(EditorRegistry * registry, QObject * parent = nullptr);

    ~OperationContext() override;

    /**
     * @brief Request output from a producer widget type
     *
     * If no instance of producer_type exists and it's allowed to be created,
     * one will be created and focused.
     *
     * If there's already a pending operation for this producer_type,
     * the old one is closed with reason Superseded.
     *
     * @param requester Who is making the request
     * @param producer_type What type of widget should produce the output
     * @param options Request options (channel, close behavior)
     * @return The pending operation, or nullopt if request failed
     */
    [[nodiscard]] std::optional<PendingOperation> requestOperation(
        EditorInstanceId requester,
        EditorTypeId producer_type,
        OperationRequestOptions options = {});

    /**
     * @brief Check if there's a pending operation for a producer type
     *
     * Producers call this in their "apply" handlers to know whether
     * to route output to a requester vs. normal behavior.
     *
     * @param producer_type The producer type to check
     * @return The pending operation if one exists
     */
    [[nodiscard]] std::optional<PendingOperation>
    pendingOperationFor(EditorTypeId const & producer_type) const;

    /**
     * @brief Get the requester for a pending operation
     *
     * Useful for UI display (showing who will receive the output).
     *
     * @param producer_type The producer type to check
     * @return The requester's instance ID if there's a pending operation
     */
    [[nodiscard]] std::optional<EditorInstanceId>
    requesterFor(EditorTypeId const & producer_type) const;

    /**
     * @brief Deliver a result for a pending operation
     *
     * Called by the producer when it has output ready.
     * Emits operationDelivered signal.
     *
     * @param producer_type The producing widget's type
     * @param result The result data
     * @return true if there was a pending operation and result was delivered
     */
    bool deliverResult(EditorTypeId const & producer_type, OperationResult result);

    /**
     * @brief Explicitly close an operation by ID
     *
     * @param id The operation to close
     * @param reason Why the operation is being closed
     */
    void closeOperation(OperationId const & id,
                        OperationCloseReason reason = OperationCloseReason::Explicit);

    /**
     * @brief Close any pending operation for a producer type
     *
     * @param producer_type The producer type
     * @param reason Why the operation is being closed
     */
    void closeOperationsFor(EditorTypeId const & producer_type,
                            OperationCloseReason reason);

    /**
     * @brief Close any pending operations from a requester
     *
     * Called when a requester widget is being destroyed.
     *
     * @param requester The requester's instance ID
     * @param reason Why the operation is being closed
     */
    void closeOperationsFrom(EditorInstanceId const & requester,
                             OperationCloseReason reason);

    /**
     * @brief Check if an operation is currently pending
     *
     * @param id The operation ID to check
     * @return true if the operation exists and is pending
     */
    [[nodiscard]] bool hasOperation(OperationId const & id) const;

    /**
     * @brief Get an operation by ID
     *
     * @param id The operation ID
     * @return The operation if it exists
     */
    [[nodiscard]] std::optional<PendingOperation>
    operation(OperationId const & id) const;

    /**
     * @brief Get the number of pending operations
     */
    [[nodiscard]] size_t pendingCount() const;

signals:
    /**
     * @brief Emitted when an operation is requested
     *
     * EditorRegistry listens to this to open/focus producer widgets.
     *
     * @param op The requested operation
     */
    void operationRequested(EditorLib::PendingOperation const & op);

    /**
     * @brief Emitted when a result is delivered
     *
     * Consumers connect to this to receive results.
     *
     * @param op The operation that was fulfilled
     * @param result The delivered data
     */
    void operationDelivered(EditorLib::PendingOperation const & op,
                            EditorLib::OperationResult const & result);

    /**
     * @brief Emitted when an operation closes (with or without delivery)
     *
     * @param id The operation ID
     * @param reason Why the operation closed
     */
    void operationClosed(EditorLib::OperationId const & id,
                         EditorLib::OperationCloseReason reason);

    /**
     * @brief Emitted when the pending operation for a producer changes
     *
     * Producers connect to update their UI (show target indicator).
     *
     * @param producer_type The producer type whose pending operation changed
     */
    void pendingOperationChanged(EditorLib::EditorTypeId const & producer_type);

public slots:
    /**
     * @brief Handle selection changes from SelectionContext
     *
     * Closes operations that have close_on_selection_change set.
     */
    void onSelectionChanged();

    /**
     * @brief Handle editor unregistration
     *
     * Closes operations involving the unregistered editor.
     *
     * @param instance_id The unregistered editor's ID
     */
    void onEditorUnregistered(EditorInstanceId const & instance_id);

private:
    EditorRegistry * _registry;

    // Pending operations keyed by producer type (only one per producer)
    std::map<EditorTypeId, PendingOperation> _pending_by_producer;

    // Index for lookup by operation ID
    std::map<OperationId, EditorTypeId> _id_to_producer;

    void removeOperation(EditorTypeId const & producer_type);
};

}  // namespace EditorLib

// Qt metatype registration for signal/slot use
Q_DECLARE_METATYPE(EditorLib::PendingOperation)
Q_DECLARE_METATYPE(EditorLib::OperationResult)
Q_DECLARE_METATYPE(EditorLib::OperationId)
Q_DECLARE_METATYPE(EditorLib::EditorTypeId)
Q_DECLARE_METATYPE(EditorLib::OperationCloseReason)

#endif  // OPERATION_CONTEXT_HPP

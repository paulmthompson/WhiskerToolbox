#include "OperationContext.hpp"

#include <iostream>

namespace EditorLib {

OperationContext::OperationContext(EditorRegistry * registry, QObject * parent)
    : QObject(parent)
    , _registry(registry) {
}

OperationContext::~OperationContext() = default;

std::optional<PendingOperation> OperationContext::requestOperation(
    EditorInstanceId requester,
    EditorTypeId producer_type,
    OperationRequestOptions options) {

    if (!requester.isValid()) {
        std::cerr << "OperationContext::requestOperation: Invalid requester ID" << std::endl;
        return std::nullopt;
    }

    if (!producer_type.isValid()) {
        std::cerr << "OperationContext::requestOperation: Invalid producer type" << std::endl;
        return std::nullopt;
    }

    // Close any existing operation for this producer
    if (_pending_by_producer.contains(producer_type)) {
        closeOperationsFor(producer_type, OperationCloseReason::Superseded);
    }

    // Create new operation
    PendingOperation op{
        .id = OperationId::generate(),
        .requester = std::move(requester),
        .producer_type = producer_type,
        .channel = std::move(options.channel),
        .close_on_selection_change = options.close_on_selection_change,
        .close_after_delivery = options.close_after_delivery};

    // Store operation
    _id_to_producer[op.id] = producer_type;
    _pending_by_producer[producer_type] = op;

    emit operationRequested(op);
    emit pendingOperationChanged(producer_type);

    return op;
}

std::optional<PendingOperation>
OperationContext::pendingOperationFor(EditorTypeId const & producer_type) const {
    auto it = _pending_by_producer.find(producer_type);
    if (it != _pending_by_producer.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<EditorInstanceId>
OperationContext::requesterFor(EditorTypeId const & producer_type) const {
    auto it = _pending_by_producer.find(producer_type);
    if (it != _pending_by_producer.end()) {
        return it->second.requester;
    }
    return std::nullopt;
}

bool OperationContext::deliverResult(EditorTypeId const & producer_type, 
                                     OperationResult result) {
    auto it = _pending_by_producer.find(producer_type);
    if (it == _pending_by_producer.end()) {
        std::cerr << "OperationContext::deliverResult: No pending operation for producer: "
                  << producer_type.toStdString() << std::endl;
        return false;
    }

    PendingOperation const op = it->second;

    // Emit delivery signal
    emit operationDelivered(op, result);

    // Close if configured to do so
    if (op.close_after_delivery) {
        closeOperation(op.id, OperationCloseReason::Delivered);
    }

    return true;
}

void OperationContext::closeOperation(OperationId const & id, OperationCloseReason reason) {
    auto id_it = _id_to_producer.find(id);
    if (id_it == _id_to_producer.end()) {
        return;
    }

    EditorTypeId producer_type = id_it->second;
    removeOperation(producer_type);

    emit operationClosed(id, reason);
    emit pendingOperationChanged(producer_type);
}

void OperationContext::closeOperationsFor(EditorTypeId const & producer_type,
                                          OperationCloseReason reason) {
    auto it = _pending_by_producer.find(producer_type);
    if (it == _pending_by_producer.end()) {
        return;
    }

    OperationId id = it->second.id;
    removeOperation(producer_type);

    emit operationClosed(id, reason);
    emit pendingOperationChanged(producer_type);
}

void OperationContext::closeOperationsFrom(EditorInstanceId const & requester,
                                           OperationCloseReason reason) {
    // Find all operations from this requester
    std::vector<EditorTypeId> to_close;
    for (auto const & [producer_type, op] : _pending_by_producer) {
        if (op.requester == requester) {
            to_close.push_back(producer_type);
        }
    }

    // Close them
    for (auto const & producer_type : to_close) {
        auto it = _pending_by_producer.find(producer_type);
        if (it != _pending_by_producer.end()) {
            OperationId id = it->second.id;
            removeOperation(producer_type);
            emit operationClosed(id, reason);
            emit pendingOperationChanged(producer_type);
        }
    }
}

bool OperationContext::hasOperation(OperationId const & id) const {
    return _id_to_producer.contains(id);
}

std::optional<PendingOperation> OperationContext::operation(OperationId const & id) const {
    auto id_it = _id_to_producer.find(id);
    if (id_it == _id_to_producer.end()) {
        return std::nullopt;
    }

    auto op_it = _pending_by_producer.find(id_it->second);
    if (op_it != _pending_by_producer.end()) {
        return op_it->second;
    }
    return std::nullopt;
}

size_t OperationContext::pendingCount() const {
    return _pending_by_producer.size();
}

void OperationContext::onSelectionChanged() {
    // Close operations that requested close on selection change
    std::vector<EditorTypeId> to_close;
    for (auto const & [producer_type, op] : _pending_by_producer) {
        if (op.close_on_selection_change) {
            to_close.push_back(producer_type);
        }
    }

    for (auto const & producer_type : to_close) {
        closeOperationsFor(producer_type, OperationCloseReason::SelectionChanged);
    }
}

void OperationContext::onEditorUnregistered(EditorInstanceId const & instance_id) {
    // Close operations where this editor is the requester
    closeOperationsFrom(instance_id, OperationCloseReason::RequesterClosed);

    // Note: We don't track producer instances, only producer types.
    // If you need to close operations when a specific producer instance closes,
    // that would require additional tracking.
}

void OperationContext::removeOperation(EditorTypeId const & producer_type) {
    auto it = _pending_by_producer.find(producer_type);
    if (it != _pending_by_producer.end()) {
        _id_to_producer.erase(it->second.id);
        _pending_by_producer.erase(it);
    }
}

}  // namespace EditorLib

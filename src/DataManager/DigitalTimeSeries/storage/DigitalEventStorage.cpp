/**
 * @file DigitalEventStorage.cpp
 * @brief Implementation helpers for digital event storage wrapper and backends.
 */

#include "DigitalEventStorage.hpp"

#include "OwningDigitalEventStorage.hpp"
#include "ViewDigitalEventStorage.hpp"

DigitalEventStorageWrapper::DigitalEventStorageWrapper()
    : _impl(std::make_shared<StorageModel<OwningDigitalEventStorage>>(OwningDigitalEventStorage{})) {}

OwningDigitalEventStorage * DigitalEventStorageWrapper::tryGetMutableOwning() {
    return tryGet<OwningDigitalEventStorage>();
}

OwningDigitalEventStorage const * DigitalEventStorageWrapper::tryGetOwning() const {
    return tryGet<OwningDigitalEventStorage>();
}

std::shared_ptr<OwningDigitalEventStorage const> DigitalEventStorageWrapper::getSharedOwningStorage() const {
    if (auto const * view_model = dynamic_cast<StorageModel<ViewDigitalEventStorage> const *>(_impl.get())) {
        return view_model->_storage.source();
    }

    if (auto owning_model = std::dynamic_pointer_cast<StorageModel<OwningDigitalEventStorage> const>(_impl)) {
        return std::shared_ptr<OwningDigitalEventStorage const>(owning_model, &owning_model->_storage);
    }

    return nullptr;
}

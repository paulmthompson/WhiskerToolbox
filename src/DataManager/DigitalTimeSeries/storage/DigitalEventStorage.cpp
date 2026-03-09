#include "DigitalEventStorage.hpp"

#include "OwningDigitalEventStorage.hpp"
#include "ViewDigitalEventStorage.hpp"

DigitalEventStorageWrapper::DigitalEventStorageWrapper()
    : _impl(std::make_shared<StorageModel<OwningDigitalEventStorage>>(
              OwningDigitalEventStorage{})) {}

OwningDigitalEventStorage * DigitalEventStorageWrapper::tryGetMutableOwning() {
    return tryGet<OwningDigitalEventStorage>();
}

OwningDigitalEventStorage const * DigitalEventStorageWrapper::tryGetOwning() const {
    return tryGet<OwningDigitalEventStorage>();
}

std::shared_ptr<OwningDigitalEventStorage const> DigitalEventStorageWrapper::getSharedOwningStorage() const {
    // Check if we have view storage - return its existing source
    if (auto const * view_model = dynamic_cast<StorageModel<ViewDigitalEventStorage> const *>(_impl.get())) {
        return view_model->_storage.source();
    }

    // Check if we have owning storage - use aliasing constructor for zero-copy sharing
    if (auto owning_model = std::dynamic_pointer_cast<StorageModel<OwningDigitalEventStorage> const>(_impl)) {
        // Aliasing constructor: shares ownership with _impl but points to the inner storage
        return std::shared_ptr<OwningDigitalEventStorage const>(owning_model, &owning_model->_storage);
    }

    // Lazy storage - no shared owning storage available
    return nullptr;
}


#include "DigitalIntervalStorage.hpp"

#include "OwningDigitalIntervalStorage.hpp"
#include "ViewDigitalIntervalStorage.hpp"

DigitalIntervalStorageWrapper::DigitalIntervalStorageWrapper()
    : _impl(std::make_shared<StorageModel<OwningDigitalIntervalStorage>>(
              OwningDigitalIntervalStorage{})) {}


OwningDigitalIntervalStorage * DigitalIntervalStorageWrapper::tryGetMutableOwning() {
    return tryGet<OwningDigitalIntervalStorage>();
}

OwningDigitalIntervalStorage const * DigitalIntervalStorageWrapper::tryGetOwning() const {
    return tryGet<OwningDigitalIntervalStorage>();
}


std::shared_ptr<OwningDigitalIntervalStorage const> DigitalIntervalStorageWrapper::getSharedOwningStorage() const {
    // Check if we have view storage - return its existing source
    if (auto const * view_model = dynamic_cast<StorageModel<ViewDigitalIntervalStorage> const *>(_impl.get())) {
        return view_model->_storage.source();
    }

    // Check if we have owning storage - use aliasing constructor for zero-copy sharing
    if (auto owning_model = std::dynamic_pointer_cast<StorageModel<OwningDigitalIntervalStorage> const>(_impl)) {
        // Aliasing constructor: shares ownership with _impl but points to the inner storage
        return std::shared_ptr<OwningDigitalIntervalStorage const>(owning_model, &owning_model->_storage);
    }

    // Lazy storage - no shared owning storage available
    return nullptr;
}
#ifndef WHISKERTOOLBOX_TENSORS_INVALIDATION_WIRING_FN_HPP
#define WHISKERTOOLBOX_TENSORS_INVALIDATION_WIRING_FN_HPP

#include <functional>

class LazyColumnTensorStorage;
class TensorData;

/**
 * @brief Callback that wires observer invalidation after lazy tensor construction.
 *
 * Receives the lazy storage (for invalidateColumn) and the TensorData
 * (for notifyObservers). Called exactly once, at the end of createFromLazyColumns().
 * Constructed at the builder layer (e.g., TensorColumnBuilders.hpp in TransformsV2).
 *
 */
using InvalidationWiringFn = std::function<void(LazyColumnTensorStorage &, TensorData &)>;


#endif// WHISKERTOOLBOX_TENSORS_INVALIDATION_WIRING_FN_HPP
#ifndef GATHER_RESULT_ROW_CONTEXT_HPP
#define GATHER_RESULT_ROW_CONTEXT_HPP

/**
 * @file GatherResultRowContext.hpp
 * @brief TransformsV2 helpers for row-wise GatherResult processing.
 */

#include "GatherResult/GatherResult.hpp"
#include "GatherResult/ViewAdaptorTypes.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "TransformsV2/PipelineValueStore/PipelineValueStore.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/extension/ValueProjectionTypes.hpp"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <memory>
#include <numeric>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Neuralyzer::Gather {

/**
 * @brief Build a TransformsV2 value store for one gathered row.
 *
 * @tparam T GatherResult row DataObject type
 * @param gather GatherResult supplying row metadata
 * @param row_index Visible row index, respecting any GatherResult reorder mapping
 * @return PipelineValueStore populated with row metadata
 *
 * @pre @p row_index must be less than `gather.size()`.
 * @pre `gather` must have interval metadata for @p row_index.
 * @post Returned store contains `alignment_time`, `trial_index`, `trial_duration`, and `end_time`.
 */
template<typename T>
[[nodiscard]] Neuralyzer::Transforms::V2::PipelineValueStore buildGatherRowStore(
        GatherResult<T> const & gather,
        typename GatherResult<T>::size_type row_index) {
    if (row_index >= gather.size()) {
        throw std::out_of_range("buildGatherRowStore: row index out of range");
    }

    auto const interval = gather.intervalAtReordered(row_index);
    auto const alignment_time = gather.alignmentTimeAt(row_index);
    auto const original_index = gather.originalIndex(row_index);

    Neuralyzer::Transforms::V2::PipelineValueStore store;
    store.set("alignment_time", alignment_time.getValue());
    store.set("trial_index", static_cast<int64_t>(original_index));
    store.set("trial_duration", (interval.end - interval.start).getValue());
    store.set("end_time", interval.end.getValue());
    return store;
}

/**
 * @brief Project each gathered row with a row-context-aware projection factory.
 *
 * @tparam T GatherResult row DataObject type
 * @tparam Value Projected value type
 * @param gather GatherResult supplying rows and metadata
 * @param factory Factory that binds one projection per row store
 * @return One projection function per row
 *
 * @pre @p factory must return a valid projection for each row store.
 * @post Return size equals `gather.size()`.
 */
template<typename T, typename Value>
[[nodiscard]] std::vector<Neuralyzer::Transforms::V2::ValueProjectionFn<typename GatherResult<T>::element_type, Value>>
projectGatherRows(
        GatherResult<T> const & gather,
        Neuralyzer::Transforms::V2::ValueProjectionFactoryV2<typename GatherResult<T>::element_type, Value> const & factory) {
    using ProjectionFn = Neuralyzer::Transforms::V2::ValueProjectionFn<typename GatherResult<T>::element_type, Value>;

    std::vector<ProjectionFn> projections;
    projections.reserve(gather.size());

    for (typename GatherResult<T>::size_type i = 0; i < gather.size(); ++i) {
        projections.push_back(factory(buildGatherRowStore(gather, i)));
    }

    return projections;
}

/**
 * @brief Reduce each gathered row with a row-context-aware reducer factory.
 *
 * @tparam T GatherResult row DataObject type
 * @tparam Scalar Scalar reduction output type
 * @param gather GatherResult supplying rows and metadata
 * @param reducer_factory Factory that binds one reducer per row store
 * @return One scalar value per row
 *
 * @pre Each row DataObject must expose `view()`.
 * @pre @p reducer_factory must return a valid reducer for each row store.
 * @post Return size equals `gather.size()`.
 */
template<typename T, typename Scalar>
[[nodiscard]] std::vector<Scalar> reduceGatherRows(
        GatherResult<T> const & gather,
        ReducerFactoryV2<typename GatherResult<T>::element_type, Scalar> const & reducer_factory) {
    using Element = typename GatherResult<T>::element_type;

    std::vector<Scalar> results;
    results.reserve(gather.size());

    for (typename GatherResult<T>::size_type i = 0; i < gather.size(); ++i) {
        auto reducer = reducer_factory(buildGatherRowStore(gather, i));
        auto view = gather[i]->view();
        std::vector<Element> elements(view.begin(), view.end());
        results.push_back(reducer(std::span<Element const>{elements}));
    }

    return results;
}

/**
 * @brief Return row indices sorted by a row-context-aware reduction.
 *
 * @tparam T GatherResult row DataObject type
 * @tparam Scalar Comparable scalar reduction output type
 * @param gather GatherResult supplying rows and metadata
 * @param reducer_factory Factory that binds one reducer per row store
 * @param ascending Sort smallest values first when true
 * @return Row indices that sort the reduction outputs
 *
 * @pre @p reducer_factory must return comparable values.
 * @post NaN floating-point values sort to the end, matching the former core helper.
 */
template<typename T, typename Scalar>
[[nodiscard]] std::vector<typename GatherResult<T>::size_type> sortGatherRowsBy(
        GatherResult<T> const & gather,
        ReducerFactoryV2<typename GatherResult<T>::element_type, Scalar> const & reducer_factory,
        bool ascending = true) {
    using size_type = typename GatherResult<T>::size_type;

    auto values = reduceGatherRows<T, Scalar>(gather, reducer_factory);

    std::vector<size_type> indices(gather.size());
    std::iota(indices.begin(), indices.end(), size_type{0});

    auto const compare_ascending = [&values](size_type left, size_type right) {
        if constexpr (std::is_floating_point_v<Scalar>) {
            if (std::isnan(values[left])) return false;
            if (std::isnan(values[right])) return true;
        }
        return values[left] < values[right];
    };
    auto const compare_descending = [&values](size_type left, size_type right) {
        if constexpr (std::is_floating_point_v<Scalar>) {
            if (std::isnan(values[left])) return false;
            if (std::isnan(values[right])) return true;
        }
        return values[left] > values[right];
    };

    if (ascending) {
        std::stable_sort(indices.begin(), indices.end(), compare_ascending);
    } else {
        std::stable_sort(indices.begin(), indices.end(), compare_descending);
    }

    return indices;
}

/**
 * @brief Execute a TransformsV2 pipeline for each row and return row-aligned DataObjects.
 *
 * @tparam InputT Input GatherResult row DataObject type
 * @tparam OutputT Pipeline output DataObject type
 * @param gather GatherResult supplying input rows and metadata
 * @param pipeline TransformPipeline executed once per row
 * @return GatherResult containing one output DataObject per input row
 *
 * @pre @p pipeline must produce `std::shared_ptr<OutputT>` for every row.
 * @post Returned GatherResult preserves @p gather row metadata and ordering.
 */
template<typename InputT, typename OutputT>
[[nodiscard]] GatherResult<OutputT> transformGatherRows(
        GatherResult<InputT> const & gather,
        Neuralyzer::Transforms::V2::TransformPipeline const & pipeline) {
    std::vector<std::shared_ptr<OutputT>> rows;
    rows.reserve(gather.size());

    for (typename GatherResult<InputT>::size_type i = 0; i < gather.size(); ++i) {
        DataTypeVariant const row_input{gather[i]};
        auto const row_output = Neuralyzer::Transforms::V2::executePipeline(row_input, pipeline);
        rows.push_back(std::get<std::shared_ptr<OutputT>>(row_output));
    }

    return GatherResult<OutputT>::fromRowsLike(gather, std::move(rows));
}

}// namespace Neuralyzer::Gather

#endif// GATHER_RESULT_ROW_CONTEXT_HPP

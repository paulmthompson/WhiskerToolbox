/**
 * @file ResultProcessor.cpp
 * @brief Implementation of result merging for batch inference.
 */

#include "ResultProcessor.hpp"

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/WriteReservation.hpp"

#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <QTimer>

#include <algorithm>
#include <set>

// ════════════════════════════════════════════════════════════════════════════
// ResultProcessor::Impl
// ════════════════════════════════════════════════════════════════════════════

struct ResultProcessor::Impl {
    std::shared_ptr<DataManager> _dm;
    std::shared_ptr<DeepLearningState> _state;
    std::shared_ptr<WriteReservation> _write_reservation;
    QTimer * _merge_timer = nullptr;

    std::map<std::string, std::vector<std::pair<int, std::vector<float>>>>
            _pending_feature_rows;
};

// ════════════════════════════════════════════════════════════════════════════
// ResultProcessor
// ════════════════════════════════════════════════════════════════════════════

ResultProcessor::ResultProcessor(std::shared_ptr<DataManager> dm,
                                 std::shared_ptr<DeepLearningState> state,
                                 QObject * parent)
    : QObject(parent),
      _impl(std::make_unique<Impl>()) {
    _impl->_dm = std::move(dm);
    _impl->_state = std::move(state);
}

ResultProcessor::~ResultProcessor() {
    stopMergeTimer();
}

void ResultProcessor::acceptResults(std::vector<FrameResult> results) {
    std::set<std::string> affected_keys;

    for (auto & fr: results) {
        TimeFrameIndex const frame_idx(fr.frame_index);
        std::visit(
                [&](auto && decoded) {
                    using T = std::decay_t<decltype(decoded)>;
                    if constexpr (std::is_same_v<T, Mask2D>) {
                        auto mask_data =
                                _impl->_dm->getData<MaskData>(fr.data_key);
                        if (!mask_data) {
                            _impl->_dm->setData<MaskData>(fr.data_key,
                                                          TimeKey("media"));
                            mask_data =
                                    _impl->_dm->getData<MaskData>(fr.data_key);
                        }
                        if (mask_data) {
                            mask_data->addAtTime(
                                    frame_idx,
                                    std::forward<decltype(decoded)>(decoded),
                                    NotifyObservers::No);
                            affected_keys.insert(fr.data_key);
                        }
                    } else if constexpr (std::is_same_v<T, Point2D<float>>) {
                        auto point_data =
                                _impl->_dm->getData<PointData>(fr.data_key);
                        if (!point_data) {
                            _impl->_dm->setData<PointData>(fr.data_key,
                                                           TimeKey("media"));
                            point_data =
                                    _impl->_dm->getData<PointData>(fr.data_key);
                        }
                        if (point_data) {
                            point_data->addAtTime(
                                    frame_idx,
                                    std::forward<decltype(decoded)>(decoded),
                                    NotifyObservers::No);
                            affected_keys.insert(fr.data_key);
                        }
                    } else if constexpr (std::is_same_v<T, Line2D>) {
                        auto line_data =
                                _impl->_dm->getData<LineData>(fr.data_key);
                        if (!line_data) {
                            _impl->_dm->setData<LineData>(fr.data_key,
                                                          TimeKey("media"));
                            line_data =
                                    _impl->_dm->getData<LineData>(fr.data_key);
                        }
                        if (line_data) {
                            line_data->addAtTime(
                                    frame_idx,
                                    std::forward<decltype(decoded)>(decoded),
                                    NotifyObservers::No);
                            affected_keys.insert(fr.data_key);
                        }
                    } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                        _impl->_pending_feature_rows[fr.data_key].emplace_back(
                                fr.frame_index,
                                std::forward<decltype(decoded)>(decoded));
                    }
                },
                fr.data);
    }

    for (auto const & key: affected_keys) {
        if (auto d = _impl->_dm->getData<MaskData>(key)) d->notifyObservers();
        if (auto d = _impl->_dm->getData<PointData>(key)) d->notifyObservers();
        if (auto d = _impl->_dm->getData<LineData>(key)) d->notifyObservers();
    }

    if (!affected_keys.empty()) {
        emit resultsWritten(static_cast<int>(affected_keys.size()));
    }
}

void ResultProcessor::flushFeatureVectors() {
    for (auto & [key, rows]: _impl->_pending_feature_rows) {
        if (rows.empty()) continue;
        std::sort(rows.begin(), rows.end(),
                  [](auto const & a, auto const & b) { return a.first < b.first; });

        // Get or create the TensorData for this key
        auto tensor = _impl->_dm->getData<TensorData>(key);
        if (!tensor) {
            _impl->_dm->setData<TensorData>(key, TimeKey("media"));
            tensor = _impl->_dm->getData<TensorData>(key);
        }
        if (tensor) {
            auto time_frame = _impl->_dm->getTime();
            tensor->upsertRows(rows, std::move(time_frame));
        }
    }
    _impl->_pending_feature_rows.clear();
}

void ResultProcessor::clear() {
    _impl->_pending_feature_rows.clear();
}

void ResultProcessor::setReservation(
        std::shared_ptr<WriteReservation> reservation) {
    _impl->_write_reservation = std::move(reservation);
}

void ResultProcessor::startMergeTimer() {
    if (_impl->_merge_timer) return;

    _impl->_merge_timer = new QTimer(this);
    connect(_impl->_merge_timer, &QTimer::timeout, this, [this]() {
        if (!_impl->_write_reservation) return;

        auto pending = _impl->_write_reservation->drain();
        if (pending.empty()) return;

        acceptResults(std::move(pending));
    });
    _impl->_merge_timer->start(200);
}

void ResultProcessor::stopMergeTimer() {
    if (_impl->_merge_timer) {
        _impl->_merge_timer->stop();
        _impl->_merge_timer->deleteLater();
        _impl->_merge_timer = nullptr;
    }

    if (_impl->_write_reservation) {
        auto pending = _impl->_write_reservation->drain();
        if (!pending.empty()) {
            acceptResults(std::move(pending));
        }
        _impl->_write_reservation.reset();
    }
}

#ifndef KALMAN_FILTER_T_HPP
#define KALMAN_FILTER_T_HPP

#include "Filter/IFilter.hpp"

#include <Eigen/Dense>
#include <cstddef>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <sstream>
#include <optional>

namespace StateEstimation {

enum class ContractPolicy {
    Throw,
    LogAndContinue,
    Abort
};

struct KalmanDiagnostics {
    std::size_t dimensionMismatches = 0;
};

constexpr double kSymmetrizeHalf = 0.5;

/**
 * @brief Fixed-size Kalman filter for tests and high-assurance contexts.
 *
 * Provides compile-time sized state and measurement dimensions to catch
 * shape errors early while conforming to the dynamic `IFilter` API.
 */
template <int StateDim, int MeasDim>
class KalmanFilterT final : public IFilter {
public:
    using StateVec = Eigen::Matrix<double, StateDim, 1>;
    using StateMat = Eigen::Matrix<double, StateDim, StateDim>;
    using MeasVec  = Eigen::Matrix<double, MeasDim, 1>;
    using MeasMat  = Eigen::Matrix<double, MeasDim, StateDim>;
    using MeasCov  = Eigen::Matrix<double, MeasDim, MeasDim>;

    KalmanFilterT(StateMat const & F,
                  MeasMat const & H,
                  StateMat const & Q,
                  MeasCov const & R,
                  ContractPolicy policy = ContractPolicy::Throw)
        : F_(F), H_(H), Q_(Q), R_(R), x_(), P_(), policy_(policy) {
        // Validate static configuration dimensions early
        bool const okF = (F_.rows() == F_.cols());
        bool const okQ = (Q_.rows() == F_.rows()) && (Q_.cols() == F_.cols());
        bool const okH = (H_.cols() == F_.cols());
        bool const okR = (R_.rows() == H_.rows()) && (R_.cols() == H_.rows());
        if (!(okF && okQ && okH && okR)) {
            std::ostringstream oss;
            oss << "Invalid Kalman config: F=" << F_.rows() << "x" << F_.cols()
                << ", Q=" << Q_.rows() << "x" << Q_.cols()
                << ", H=" << H_.rows() << "x" << H_.cols()
                << ", R=" << R_.rows() << "x" << R_.cols();
            switch (policy_) {
                case ContractPolicy::Throw:
                    throw std::logic_error(oss.str());
                case ContractPolicy::Abort:
                    spdlog::critical("{}", oss.str());
                    std::abort();
                case ContractPolicy::LogAndContinue:
                default:
                    spdlog::error(oss.str());
            }
        }
        // Ensure sizes for dynamic instantiations; noop for fixed-size
        if constexpr (StateDim == Eigen::Dynamic) {
            x_.resize(F_.rows());
            P_.resize(F_.rows(), F_.rows());
        }
        x_.setZero();
        P_.setIdentity();
    }

    void initialize(FilterState const & initial_state) override {
        enforceDims(initial_state);
        x_ = initial_state.state_mean;
        P_ = initial_state.state_covariance;
    }

    FilterState predict() override {
        x_ = F_ * x_;
        P_ = F_ * P_ * F_.transpose() + Q_;
        // Symmetrize to counteract numerical asymmetry
        P_ = (P_ + P_.transpose()) * kSymmetrizeHalf;
        return toFilterState();
    }

    FilterState update(FilterState const & predicted_state, Measurement const & measurement) override {
        return update(predicted_state, measurement, 1.0);
    }

    FilterState update(FilterState const & predicted_state, Measurement const & measurement, double noise_scale_factor) override {
        enforceDims(predicted_state);
        if (measurement.feature_vector.size() != measDim()) {
            failContract("Measurement vector wrong size", predicted_state, measurement);
            return toFilterState();
        }

        StateVec x_pred = predicted_state.state_mean;
        StateMat P_pred = predicted_state.state_covariance;
        MeasVec z = measurement.feature_vector;

        MeasCov const R_scaled = static_cast<double>(noise_scale_factor) * R_;
        MeasVec const y = z - H_ * x_pred;
        MeasCov S = H_ * P_pred * H_.transpose() + R_scaled;
        Eigen::Matrix<double, StateDim, MeasDim> K = P_pred * H_.transpose() * S.inverse();

        x_ = x_pred + K * y;
        StateMat I;
        if constexpr (StateDim == Eigen::Dynamic) {
            I.resize(F_.rows(), F_.rows());
        }
        I.setIdentity();
        StateMat A = I - K * H_;
        P_ = A * P_pred * A.transpose() + K * R_scaled * K.transpose();
        P_ = (P_ + P_.transpose()) * kSymmetrizeHalf;
        return toFilterState();
    }

    std::vector<FilterState> smooth(std::vector<FilterState> const & forward_states) override {
        if (forward_states.empty()) return {};
        std::vector<FilterState> smoothed = forward_states;
        for (std::size_t k = forward_states.size() - 1; k-- > 0; ) {
            FilterState const & fwd_k = forward_states[k];
            FilterState const & sm_k1 = smoothed[k + 1];
            enforceDims(fwd_k);
            enforceDims(sm_k1);
            StateVec xk = fwd_k.state_mean;
            StateMat Pk = fwd_k.state_covariance;
            StateVec x_pred = F_ * xk;
            StateMat P_pred = F_ * Pk * F_.transpose() + Q_;
            P_pred = (P_pred + P_pred.transpose()) * kSymmetrizeHalf;
            Eigen::Matrix<double, StateDim, StateDim> Ck = Pk * F_.transpose() * P_pred.inverse();
            StateVec x_sm = xk + Ck * (sm_k1.state_mean - x_pred);
            StateMat P_sm = Pk + Ck * (sm_k1.state_covariance - P_pred) * Ck.transpose();
            P_sm = (P_sm + P_sm.transpose()) * kSymmetrizeHalf;
            smoothed[k].state_mean = x_sm;
            smoothed[k].state_covariance = P_sm;
        }
        return smoothed;
    }

    [[nodiscard]] FilterState getState() const override { return toFilterState(); }

    [[nodiscard]] std::unique_ptr<IFilter> clone() const override {
        return std::make_unique<KalmanFilterT<StateDim, MeasDim>>(*this);
    }

private:
    StateMat F_;
    MeasMat H_;
    StateMat Q_;
    MeasCov R_;
    StateVec x_;
    StateMat P_;
    ContractPolicy policy_;
    KalmanDiagnostics diagnostics_{};

    void enforceDims(FilterState const & s) const {
        int const exp = stateDim();
        bool ok = (s.state_mean.size() == exp) &&
                  (s.state_covariance.rows() == exp) &&
                  (s.state_covariance.cols() == exp);
        if (!ok) {
            std::ostringstream oss;
            oss << "State dimension mismatch: mean=" << s.state_mean.size()
                << ", cov=" << s.state_covariance.rows() << "x" << s.state_covariance.cols()
                << ", expected=" << exp;
            failContract(oss.str(), s, std::nullopt);
        }
    }

    void failContract(std::string const & msg, FilterState const &, std::optional<Measurement> const & m) const {
        std::ostringstream oss;
        oss << msg;
        if (m.has_value()) {
            oss << ", measSize=" << m->feature_vector.size() << ", expected=" << MeasDim;
        }
        switch (policy_) {
            case ContractPolicy::Throw:
                throw std::logic_error(oss.str());
            case ContractPolicy::Abort:
                spdlog::critical("{}", oss.str());
                std::abort();
            case ContractPolicy::LogAndContinue:
            default:
                spdlog::error(oss.str());
        }
    }

    [[nodiscard]] FilterState toFilterState() const {
        FilterState s{};
        int const n = stateDim();
        s.state_mean = Eigen::VectorXd::Zero(n);
        s.state_covariance = Eigen::MatrixXd::Zero(n, n);
        s.state_mean = x_;
        s.state_covariance = P_;
        return s;
    }

    [[nodiscard]] int stateDim() const { return static_cast<int>(F_.rows()); }
    [[nodiscard]] int measDim() const { return static_cast<int>(H_.rows()); }
};

} // namespace StateEstimation

#endif // KALMAN_FILTER_T_HPP



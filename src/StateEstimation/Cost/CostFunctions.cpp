#include "CostFunctions.hpp"

#include "Filter/Kalman/KalmanMatrixBuilder.hpp"

#include <chrono>
#include <iostream>
#include <sstream>

namespace StateEstimation {

CostFunction createMahalanobisCostFunction(Eigen::MatrixXd const & H,
                                           Eigen::MatrixXd const & R) {
    return [H, R](FilterState const & predicted_state,
                  Eigen::VectorXd const & observation,
                  int /* num_gap_frames */) -> double {
        constexpr double kInnovationRegEps = 1e-6;
        constexpr double kLargeInvalidAssociationCost = 1e5;
        constexpr double kSvdTolScale = 1e-10;
        constexpr double kTinyDenomEps = 1e-20;

        Eigen::VectorXd innovation = observation - (H * predicted_state.state_mean);
        Eigen::MatrixXd innovation_covariance = H * predicted_state.state_covariance * H.transpose() + R;

        // Regularize to prevent singularity
        innovation_covariance.diagonal().array() += kInnovationRegEps;

        // Use LLT (Cholesky) decomposition for numerical stability with cross-correlated features
        Eigen::LLT<Eigen::MatrixXd> llt(innovation_covariance);

        if (llt.info() == Eigen::Success) {
            Eigen::VectorXd solved = llt.solve(innovation);
            double dist_sq = innovation.transpose() * solved;

            if (std::isfinite(dist_sq) && dist_sq >= 0.0) {
                return std::sqrt(dist_sq);
            }
        }

        // Fallback: pseudo-inverse for ill-conditioned matrices
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(innovation_covariance,
                                              Eigen::ComputeThinU | Eigen::ComputeThinV);

        double tolerance = kSvdTolScale * svd.singularValues()(0);
        Eigen::VectorXd inv_singular_values = svd.singularValues();
        int num_zero_singular_values = 0;
        for (int i = 0; i < inv_singular_values.size(); ++i) {
            if (inv_singular_values(i) > tolerance) {
                inv_singular_values(i) = 1.0 / inv_singular_values(i);
            } else {
                inv_singular_values(i) = 0.0;
                num_zero_singular_values++;
            }
        }

        Eigen::MatrixXd pseudo_inv = svd.matrixV() *
                                     inv_singular_values.asDiagonal() *
                                     svd.matrixU().transpose();

        double dist_sq = innovation.transpose() * pseudo_inv * innovation;

        if (!std::isfinite(dist_sq) || dist_sq < 0.0) {
            // Log diagnostic information about the numerical failure
            static thread_local int failure_count = 0;
            static thread_local std::chrono::steady_clock::time_point last_log_time;
            auto now = std::chrono::steady_clock::now();

            // Throttle logging to once per second to avoid spam
            if (failure_count == 0 ||
                std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time).count() >= 1) {

                double condition_number = svd.singularValues()(0) /
                                          (svd.singularValues()(svd.singularValues().size() - 1) + kTinyDenomEps);
                double determinant = innovation_covariance.determinant();

                last_log_time = now;
                failure_count = 0;
            }
            failure_count++;

            return kLargeInvalidAssociationCost;// Large but finite distance
        }

        return std::sqrt(dist_sq);
    };
}


CostFunction createDynamicsAwareCostFunction(
        Eigen::MatrixXd const & H,
        Eigen::MatrixXd const & R,
        KalmanMatrixBuilder::StateIndexMap const & index_map,
        double dt,
        double beta,
        double gamma,
        double lambda_gap) {
    // Capture by value; matrices are unused here but kept for signature compatibility
    return [H, R, index_map, dt, beta, gamma, lambda_gap](FilterState const & predicted_state,
                                                          Eigen::VectorXd const & observation,
                                                          int num_gap_frames) -> double {
        (void) H;
        (void) R;
        if (num_gap_frames <= 0) return 0.0;

        constexpr double kMinDt = 1e-9;
        constexpr double kTwo = 2.0;
        constexpr double kHalf = 0.5;
        double const gap_dt = static_cast<double>(num_gap_frames) * std::max(dt, kMinDt);

        auto gather = [](Eigen::VectorXd const & v, std::vector<int> const & idx) -> Eigen::VectorXd {
            Eigen::VectorXd out(idx.size());
            for (size_t i = 0; i < idx.size(); ++i) out(static_cast<Eigen::Index>(i)) = v(idx[i]);
            return out;
        };
        auto gatherCov = [](Eigen::MatrixXd const & M, std::vector<int> const & idx) -> Eigen::MatrixXd {
            Eigen::MatrixXd out(idx.size(), idx.size());
            for (size_t i = 0; i < idx.size(); ++i) {
                for (size_t j = 0; j < idx.size(); ++j) {
                    out(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) = M(idx[i], idx[j]);
                }
            }
            return out;
        };

        auto mahalHalf = [](Eigen::VectorXd const & r, Eigen::MatrixXd const & S) -> double {
            constexpr double kMahalanobisFallbackPenalty = 1e4;
            Eigen::LLT<Eigen::MatrixXd> llt(S);
            if (llt.info() == Eigen::Success) {
                Eigen::VectorXd const solved = llt.solve(r);
                double const d2 = r.transpose() * solved;
                return std::isfinite(d2) && d2 >= 0.0 ? kHalf * d2 : kMahalanobisFallbackPenalty;
            }
            Eigen::JacobiSVD<Eigen::MatrixXd> svd(S, Eigen::ComputeThinU | Eigen::ComputeThinV);
            Eigen::VectorXd inv_sv = svd.singularValues();
            constexpr double kTolScale = 1e-10;
            double const tol = inv_sv.size() > 0 ? kTolScale * inv_sv(0) : kTolScale;
            for (int i = 0; i < inv_sv.size(); ++i) inv_sv(i) = inv_sv(i) > tol ? 1.0 / inv_sv(i) : 0.0;
            Eigen::MatrixXd const S_pinv = svd.matrixV() * inv_sv.asDiagonal() * svd.matrixU().transpose();
            double const d2 = r.transpose() * S_pinv * r;
            return std::isfinite(d2) && d2 >= 0.0 ? kHalf * d2 : kMahalanobisFallbackPenalty;
        };

        double cost = 0.0;
        for (auto const & feat: index_map.features) {
            if (feat.velocity_state_indices.empty() || feat.position_state_indices.empty()) continue;

            // Predicted position/velocity
            Eigen::VectorXd const x_pred_pos = gather(predicted_state.state_mean, feat.position_state_indices);
            Eigen::VectorXd const v_pred = gather(predicted_state.state_mean, feat.velocity_state_indices);

            // Observed position components for this feature
            Eigen::VectorXd z_pos(feat.position_state_indices.size());
            for (size_t i = 0; i < feat.position_state_indices.size(); ++i) {
                int const mrow = feat.measurement_indices[i];
                z_pos(static_cast<Eigen::Index>(i)) = observation(mrow);
            }

            // Velocity consistency
            Eigen::VectorXd const v_impl = (z_pos - x_pred_pos) / gap_dt;
            Eigen::MatrixXd const Sigma_v = gatherCov(predicted_state.state_covariance, feat.velocity_state_indices);
            cost += beta * mahalHalf(v_impl - v_pred, Sigma_v);

            // Implied acceleration toward zero
            Eigen::VectorXd const a_impl = kTwo * (z_pos - x_pred_pos) / (gap_dt * gap_dt);
            cost += gamma * kHalf * a_impl.squaredNorm();
        }

        if (lambda_gap > 0.0) cost += lambda_gap * static_cast<double>(num_gap_frames);
        return cost;
    };
}

}// namespace StateEstimation

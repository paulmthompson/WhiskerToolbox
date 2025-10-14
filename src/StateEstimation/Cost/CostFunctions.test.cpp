#include <catch2/catch_test_macros.hpp>
#include <Eigen/Dense>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "StateEstimation/Cost/CostFunctions.hpp"
#include "StateEstimation/Filter/Kalman/KalmanMatrixBuilder.hpp"

using namespace StateEstimation;

TEST_CASE("Mahalanobis cost - simple identity case", "[Cost][Mahalanobis]") {
    // 2D measurement, H = I, R = I, P = I, x_pred = 0, z = [1, 0]
    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(2, 2);
    Eigen::MatrixXd R = Eigen::MatrixXd::Identity(2, 2);

    FilterState pred;
    pred.state_mean = Eigen::VectorXd::Zero(2);
    pred.state_covariance = Eigen::MatrixXd::Identity(2, 2);

    Eigen::VectorXd z(2);
    z << 1.0, 0.0;

    auto costFn = createMahalanobisCostFunction(H, R);
    double cost = costFn(pred, z, /*gap*/1);

    // S = HPH^T + R = 2I; d = sqrt(z^T S^{-1} z) = sqrt(1/2)
    REQUIRE_THAT(cost, Catch::Matchers::WithinAbs(std::sqrt(0.5), 1e-6));
}

TEST_CASE("Dynamics-aware cost - constant velocity, single step (gamma=0)", "[Cost][Dynamics]") {
    // KINEMATIC_2D state [x,y,vx,vy]; measurement z = [x,y]
    // Constant velocity: x_pred=[0,0], v_pred=[1,2], z=[1,2], dt=1, gap=1
    // With beta>0, gamma=0 -> velocity residual zero => cost ~ 0
    FeatureMetadata meta = FeatureMetadata::create("kalman_features", 2, FeatureTemporalType::KINEMATIC_2D);
    auto map = KalmanMatrixBuilder::buildStateIndexMap({meta});

    Eigen::MatrixXd H(2, 4);
    H.setZero();
    H(0, 0) = 1.0;
    H(1, 1) = 1.0;
    Eigen::MatrixXd R = Eigen::MatrixXd::Identity(2, 2);

    FilterState pred;
    pred.state_mean = Eigen::VectorXd::Zero(4);
    pred.state_mean(2) = 1.0;
    pred.state_mean(3) = 2.0;
    pred.state_covariance = Eigen::MatrixXd::Identity(4, 4);

    Eigen::VectorXd z(2);
    z << 1.0, 2.0;

    auto costFn = createDynamicsAwareCostFunction(H, R, map, /*dt=*/1.0, /*beta=*/1.0, /*gamma=*/0.0, /*lambda_gap=*/0.0);
    double cost = costFn(pred, z, /*gap*/1);

    REQUIRE_THAT(cost, Catch::Matchers::WithinAbs(0.0, 1e-9));
}

TEST_CASE("Dynamics-aware cost - constant velocity, two-step gap with small accel penalty", "[Cost][Dynamics]") {
    // gap=2, x_pred=[0,0], v_pred=[1,2], z=[2,4], so v_impl = (2,4)/2 = (1,2) -> zero vel residual
    // a_impl = 2*(2,4)/4 = (1,2) => accel term = gamma * 0.5 * (1^2 + 2^2) = 0.5*gamma*5
    FeatureMetadata meta = FeatureMetadata::create("kalman_features", 2, FeatureTemporalType::KINEMATIC_2D);
    auto map = KalmanMatrixBuilder::buildStateIndexMap({meta});

    Eigen::MatrixXd H(2, 4);
    H.setZero();
    H(0, 0) = 1.0;
    H(1, 1) = 1.0;
    Eigen::MatrixXd R = Eigen::MatrixXd::Identity(2, 2);

    FilterState pred;
    pred.state_mean = Eigen::VectorXd::Zero(4);
    pred.state_mean(2) = 1.0;
    pred.state_mean(3) = 2.0;
    pred.state_covariance = Eigen::MatrixXd::Identity(4, 4);

    Eigen::VectorXd z(2);
    z << 2.0, 4.0;

    double const gamma = 0.2;
    auto costFn = createDynamicsAwareCostFunction(H, R, map, /*dt=*/1.0, /*beta=*/1.0, /*gamma=*/gamma, /*lambda_gap=*/0.0);
    double cost = costFn(pred, z, /*gap*/2);

    REQUIRE_THAT(cost, Catch::Matchers::WithinAbs(0.5 * gamma * 5.0, 1e-9));
}



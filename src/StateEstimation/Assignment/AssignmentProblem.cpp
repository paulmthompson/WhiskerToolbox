#include "AssignmentProblem.hpp"
#include "hungarian.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace StateEstimation {

HungarianAssignment::HungarianAssignment(CostFunction cost_func)
    : cost_function_(std::move(cost_func)) {
    if (!cost_function_) {
        cost_function_ = defaultCostFunction;
    }
}

AssignmentResult HungarianAssignment::solve(std::vector<FeatureVector> const & objects,
                                            std::vector<FeatureVector> const & targets,
                                            AssignmentConstraints const & constraints) const {
    if (objects.empty()) {
        return AssignmentResult{};
    }

    auto cost_matrix = computeCostMatrix(objects, targets, constraints);
    return solve(cost_matrix, constraints);
}

AssignmentResult HungarianAssignment::solve(std::vector<std::vector<double>> const & cost_matrix,
                                            AssignmentConstraints const & constraints) const {
    if (cost_matrix.empty()) {
        return AssignmentResult{};
    }

    AssignmentResult result;
    result.assignments.resize(cost_matrix.size());
    result.costs.resize(cost_matrix.size());

    try {
        // Convert double cost matrix to integer for Hungarian algorithm
        // Hungarian algorithm requires square matrix, so pad if necessary
        size_t matrix_size = std::max(cost_matrix.size(), cost_matrix.empty() ? 0 : cost_matrix[0].size());
        std::vector<std::vector<int>> int_cost_matrix(matrix_size, std::vector<int>(matrix_size));

        double const scale_factor = 1000.0;// Scale for precision
        int const max_cost_int = std::isinf(constraints.max_cost) ? 1000000 : static_cast<int>(constraints.max_cost * scale_factor);
        int const infeasible_cost = max_cost_int + 1000;

        // Fill the matrix
        for (std::size_t i = 0; i < matrix_size; ++i) {
            for (std::size_t j = 0; j < matrix_size; ++j) {
                if (i < cost_matrix.size() && j < cost_matrix[i].size()) {
                    // Real cost from original matrix
                    if (std::isinf(cost_matrix[i][j]) || cost_matrix[i][j] > constraints.max_cost) {
                        int_cost_matrix[i][j] = infeasible_cost;
                    } else {
                        int_cost_matrix[i][j] = static_cast<int>(cost_matrix[i][j] * scale_factor);
                    }
                } else {
                    // Padding area - make it feasible but high cost so it's only used if necessary
                    int_cost_matrix[i][j] = infeasible_cost;
                }
            }
        }

        // Solve using Hungarian algorithm
        std::vector<std::vector<int>> assignment_matrix;
        int total_cost_int = Munkres::hungarian_with_assignment(int_cost_matrix, assignment_matrix);

        result.total_cost = static_cast<double>(total_cost_int) / scale_factor;

        // Extract assignments
        std::fill(result.assignments.begin(), result.assignments.end(), -1);
        for (std::size_t i = 0; i < assignment_matrix.size() && i < result.assignments.size(); ++i) {
            for (std::size_t j = 0; j < assignment_matrix[i].size(); ++j) {
                if (assignment_matrix[i][j] == 1) {
                    // Check if this is a valid assignment (not padding area)
                    if (j < cost_matrix[i].size() && !std::isinf(cost_matrix[i][j]) && cost_matrix[i][j] <= constraints.max_cost) {
                        result.assignments[i] = static_cast<int>(j);
                        result.costs[i] = cost_matrix[i][j];
                    } else {
                        result.assignments[i] = -1;// Unassigned due to cost constraint or padding
                        result.costs[i] = std::numeric_limits<double>::infinity();
                    }
                    break;
                }
            }
        }

        result.success = true;

    } catch (std::exception const & e) {
        result.success = false;
        std::fill(result.assignments.begin(), result.assignments.end(), -1);
        std::fill(result.costs.begin(), result.costs.end(), std::numeric_limits<double>::infinity());
    }

    return result;
}

double HungarianAssignment::defaultCostFunction(FeatureVector const & object, FeatureVector const & target) {
    return CostFunctions::euclideanDistance(object, target);
}

std::vector<std::vector<double>> HungarianAssignment::computeCostMatrix(
        std::vector<FeatureVector> const & objects,
        std::vector<FeatureVector> const & targets,
        AssignmentConstraints const & constraints) const {

    std::vector<std::vector<double>> cost_matrix(objects.size(), std::vector<double>(targets.size()));

    for (std::size_t i = 0; i < objects.size(); ++i) {
        for (std::size_t j = 0; j < targets.size(); ++j) {
            // Check if objects have compatible features
            bool compatible = true;
            for (auto const & required_feature: constraints.required_features) {
                if (!objects[i].hasFeature(required_feature) || !targets[j].hasFeature(required_feature)) {
                    compatible = false;
                    break;
                }
            }

            if (compatible) {
                cost_matrix[i][j] = cost_function_(objects[i], targets[j]);
            } else {
                cost_matrix[i][j] = std::numeric_limits<double>::infinity();
            }
        }
    }

    return cost_matrix;
}

namespace CostFunctions {

double euclideanDistance(FeatureVector const & object, FeatureVector const & target) {
    if (object.getDimension() != target.getDimension()) {
        return std::numeric_limits<double>::infinity();
    }

    Eigen::VectorXd diff = object.getVector() - target.getVector();
    return diff.norm();
}

MahalanobisDistance::MahalanobisDistance(Eigen::MatrixXd covariance) {
    // Add small regularization to avoid singular matrix
    covariance.diagonal().array() += 1e-6;
    covariance_inv_ = covariance.inverse();
}

double MahalanobisDistance::operator()(FeatureVector const & object, FeatureVector const & target) const {
    if (object.getDimension() != target.getDimension()) {
        return std::numeric_limits<double>::infinity();
    }

    Eigen::VectorXd diff = object.getVector() - target.getVector();
    double distance = std::sqrt(diff.transpose() * covariance_inv_ * diff);
    return distance;
}

FeatureWeightedDistance::FeatureWeightedDistance(std::unordered_map<std::string, double> weights)
    : feature_weights_(std::move(weights)) {}

double FeatureWeightedDistance::operator()(FeatureVector const & object, FeatureVector const & target) const {
    double total_distance = 0.0;
    double total_weight = 0.0;

    for (auto const & desc: object.getFeatureDescriptors()) {
        if (!target.hasFeature(desc.name)) {
            continue;// Skip features not present in target
        }

        // Only consider features that are explicitly weighted
        auto weight_it = feature_weights_.find(desc.name);
        if (weight_it == feature_weights_.end()) {
            continue;// Skip features not in weights map
        }

        double weight = weight_it->second;
        if (weight <= 0.0) {
            continue;// Skip features with zero or negative weight
        }

        Eigen::VectorXd obj_feature = object.getFeature(desc.name);
        Eigen::VectorXd target_feature = target.getFeature(desc.name);

        double feature_distance = (obj_feature - target_feature).norm();
        total_distance += weight * feature_distance * feature_distance;
        total_weight += weight;
    }

    if (total_weight == 0.0) {
        return std::numeric_limits<double>::infinity();
    }

    return std::sqrt(total_distance / total_weight);
}

}// namespace CostFunctions

}// namespace StateEstimation
#ifndef STATE_ESTIMATION_ASSIGNMENT_PROBLEM_HPP
#define STATE_ESTIMATION_ASSIGNMENT_PROBLEM_HPP

#include "Features/FeatureVector.hpp"
#include <Eigen/Dense>
#include <vector>
#include <functional>

namespace StateEstimation {

/**
 * @brief Result of an assignment operation
 */
struct AssignmentResult {
    std::vector<int> assignments;    // assignments[i] = j means object i assigned to target j, -1 for unassigned
    double total_cost;              // Total cost of the assignment
    std::vector<double> costs;      // Individual assignment costs
    bool success;                   // Whether assignment succeeded
    
    AssignmentResult() : total_cost(0.0), success(false) {}
};

/**
 * @brief Constraints for assignment operations
 */
struct AssignmentConstraints {
    double max_cost = std::numeric_limits<double>::infinity();  // Maximum allowed cost for assignment
    bool allow_unassigned = true;                               // Whether objects can remain unassigned
    bool one_to_one = true;                                    // Whether assignment must be one-to-one
    
    // Feature-specific constraints
    std::vector<std::string> required_features;                // Features that must be used in assignment
    std::vector<std::string> optional_features;                // Features that can be used if available
};

/**
 * @brief Abstract interface for solving assignment problems
 */
class AssignmentProblem {
public:
    virtual ~AssignmentProblem() = default;
    
    /**
     * @brief Solve assignment between objects and targets using feature vectors
     * @param objects Feature vectors for objects to be assigned
     * @param targets Feature vectors for assignment targets  
     * @param constraints Assignment constraints
     * @return Assignment result
     */
    virtual AssignmentResult solve(std::vector<FeatureVector> const& objects,
                                  std::vector<FeatureVector> const& targets,
                                  AssignmentConstraints const& constraints = {}) const = 0;
    
    /**
     * @brief Solve assignment using pre-computed cost matrix
     * @param cost_matrix Cost matrix where cost_matrix[i][j] is cost of assigning object i to target j
     * @param constraints Assignment constraints
     * @return Assignment result
     */
    virtual AssignmentResult solve(std::vector<std::vector<double>> const& cost_matrix,
                                  AssignmentConstraints const& constraints = {}) const = 0;
    
    /**
     * @brief Get name of the assignment algorithm
     */
    virtual std::string getName() const = 0;
};

/**
 * @brief Cost function for computing assignment costs between feature vectors
 */
using CostFunction = std::function<double(FeatureVector const& object, FeatureVector const& target)>;

/**
 * @brief Hungarian algorithm implementation for assignment
 */
class HungarianAssignment : public AssignmentProblem {
public:
    /**
     * @brief Construct with custom cost function
     * @param cost_func Function to compute cost between object and target
     */
    explicit HungarianAssignment(CostFunction cost_func = nullptr);
    
    AssignmentResult solve(std::vector<FeatureVector> const& objects,
                          std::vector<FeatureVector> const& targets,
                          AssignmentConstraints const& constraints = {}) const override;
    
    AssignmentResult solve(std::vector<std::vector<double>> const& cost_matrix,
                          AssignmentConstraints const& constraints = {}) const override;
    
    std::string getName() const override { return "Hungarian Algorithm"; }
    
    /**
     * @brief Set custom cost function
     */
    void setCostFunction(CostFunction cost_func) { cost_function_ = std::move(cost_func); }

private:
    CostFunction cost_function_;
    
    /**
     * @brief Default cost function using Euclidean distance
     */
    static double defaultCostFunction(FeatureVector const& object, FeatureVector const& target);
    
    /**
     * @brief Compute cost matrix from feature vectors
     */
    std::vector<std::vector<double>> computeCostMatrix(std::vector<FeatureVector> const& objects,
                                                      std::vector<FeatureVector> const& targets,
                                                      AssignmentConstraints const& constraints) const;
};

/**
 * @brief Common cost functions for assignment
 */
namespace CostFunctions {
    
    /**
     * @brief Euclidean distance between feature vectors
     */
    double euclideanDistance(FeatureVector const& object, FeatureVector const& target);
    
    /**
     * @brief Mahalanobis distance using provided covariance matrix
     */
    class MahalanobisDistance {
    public:
        explicit MahalanobisDistance(Eigen::MatrixXd covariance);
        double operator()(FeatureVector const& object, FeatureVector const& target) const;
        
    private:
        Eigen::MatrixXd covariance_inv_;
    };
    
    /**
     * @brief Feature-weighted distance allowing different weights for different features
     */
    class FeatureWeightedDistance {
    public:
        explicit FeatureWeightedDistance(std::unordered_map<std::string, double> weights);
        double operator()(FeatureVector const& object, FeatureVector const& target) const;
        
    private:
        std::unordered_map<std::string, double> feature_weights_;
    };
    
} // namespace CostFunctions

} // namespace StateEstimation

#endif // STATE_ESTIMATION_ASSIGNMENT_PROBLEM_HPP
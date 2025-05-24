#ifndef WHISKERTOOLBOX_MLMODELPARAMETERS_HPP
#define WHISKERTOOLBOX_MLMODELPARAMETERS_HPP

#include <memory>

// Base class for all ML model parameters
struct MLModelParametersBase {
    virtual ~MLModelParametersBase() = default;
};

// Parameters for Naive Bayes model
struct NaiveBayesParameters : public MLModelParametersBase {
    // bool incremental; // Not directly supported by mlpack NaiveBayesClassifier in a simple way for Train()
    double epsilon = 1e-9; // mlpack's NaiveBayesClassifier doesn't have a direct epsilon for Train(). This is a common parameter though.
                           // For mlpack, this might relate to variance smoothing if implementing from scratch.
                           // The current mlpack NaiveBayesClassifier is fairly basic.
};

// Parameters for Random Forest model
struct RandomForestParameters : public MLModelParametersBase {
    int numTrees = 10;
    int minimumLeafSize = 1;
    double minimumGainSplit = 1e-7; // Default in mlpack for DecisionTree which RF uses.
    int maximumDepth = 0;          // 0 means no limit in mlpack RF.
    bool warmStart = false;        // mlpack RF doesn't have a direct warm_start for Train().
                                   // This usually means reusing previous fit and adding more estimators.
    // double subsampleRatio = 1.0; // For bootstrapping, mlpack's RF does this by default.
};

#endif // WHISKERTOOLBOX_MLMODELPARAMETERS_HPP 
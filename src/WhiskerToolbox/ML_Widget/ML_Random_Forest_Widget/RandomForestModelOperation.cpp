#include "RandomForestModelOperation.hpp"
#include "ML_Widget/MLModelParameters.hpp" // For RandomForestParameters
#include "mlpack.hpp" // General mlpack include for RandomForest
#include <iostream> // For error messages

RandomForestModelOperation::RandomForestModelOperation()
    : _model(std::make_unique<mlpack::RandomForest<mlpack::GiniGain, mlpack::MultipleRandomDimensionSelect>>())
{}

std::string RandomForestModelOperation::getName() const {
    return "Random Forest";
}

std::unique_ptr<MLModelParametersBase> RandomForestModelOperation::getDefaultParameters() const {
    return std::make_unique<RandomForestParameters>();
}

bool RandomForestModelOperation::train(arma::Mat<double> const& features,
                                     arma::Row<size_t> const& labels,
                                     MLModelParametersBase const* params_base) {
    if (!_model) {
        std::cerr << "RandomForestModelOperation error: Model not initialized." << std::endl;
        return false;
    }

    const RandomForestParameters* rfParams = dynamic_cast<const RandomForestParameters*>(params_base);

    if (labels.empty()) {
        std::cerr << "RandomForestModelOperation::train: Labels cannot be empty." << std::endl;
        return false;
    }
    _numClasses = arma::max(labels) + 1;
    if (_numClasses < 2) {
        std::cout << "RandomForestModelOperation::train: Warning - only one class detected. Mlpack requires numClasses >= 2. Setting numClasses to 2." << std::endl;
        _numClasses = 2;
    }

    // Default values from RandomForestParameters struct
    int numTrees = 10;
    int minimumLeafSize = 1;
    int maximumDepth = 0; // 0 means no limit in mlpack
    // double minimumGainSplit = 1e-7; // Not directly settable on RandomForest::Train or constructor easily.
                                      // It's a property of the underlying DecisionTree.

    if (rfParams) {
        numTrees = rfParams->numTrees;
        minimumLeafSize = rfParams->minimumLeafSize;
        maximumDepth = rfParams->maximumDepth;
        // minimumGainSplit would be used if customizing the DecisionTreeType for RF.
    } else {
        std::cerr << "RandomForestModelOperation::train: Warning - RandomForestParameters not provided, using defaults." << std::endl;
    }

    try {
        // mlpack::RandomForest model(features, labels, _numClasses, numTrees, minimumLeafSize, maximumDepth);
        // The constructor trains the model. We need to assign it to _model.
        // Re-construct the model with parameters as mlpack RF trains on construction or via separate Train method
        // For parameters like minGainSplit, one might need to customize the DecisionTreeType template parameter of RandomForest
        // or mlpack would need to expose more setters.
        // Using the direct train method for simplicity if available or re-create.
        _model = std::make_unique<mlpack::RandomForest<mlpack::GiniGain, mlpack::MultipleRandomDimensionSelect>>(features, labels, _numClasses, numTrees, minimumLeafSize, maximumDepth);
        // The line above already trains the model. If there was a separate Train method with these params, it would be:
        // _model->Train(features, labels, _numClasses, numTrees, minimumLeafSize, maximumDepth);
        // However, RandomForest constructor itself performs the training with these hyperparams.
    } catch (const std::exception& e) {
        std::cerr << "RandomForestModelOperation::train failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool RandomForestModelOperation::predict(arma::Mat<double> const& features,
                                       arma::Row<size_t>& predictions) {
    if (!_model) {
        std::cerr << "RandomForestModelOperation error: Model not trained or initialized." << std::endl;
        return false;
    }
    if (features.empty()) {
        std::cerr << "RandomForestModelOperation::predict: Input features are empty." << std::endl;
        return false;
    }
    try {
        _model->Classify(features, predictions);
    } catch (const std::exception& e) {
        std::cerr << "RandomForestModelOperation::predict failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool RandomForestModelOperation::predictProbabilities(arma::Mat<double> const& features,
                                                      arma::Row<size_t>& predictions,
                                                      arma::Mat<double>& probabilities) {
    if (!_model || _numClasses == 0) {
        std::cerr << "RandomForestModelOperation error: Model not trained or numClasses not set for probability prediction." << std::endl;
        return false;
    }
    if (features.empty()) {
        std::cerr << "RandomForestModelOperation::predictProbabilities: Input features are empty." << std::endl;
        probabilities.clear();
        return false;
    }
    try {
        _model->Classify(features, predictions, probabilities); // RF Classify can also output probabilities
    } catch (const std::exception& e) {
        std::cerr << "RandomForestModelOperation::predictProbabilities failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

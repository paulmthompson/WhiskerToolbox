#include "NaiveBayesModelOperation.hpp"
#include "ML_Widget/MLModelParameters.hpp" // For NaiveBayesParameters
#include "mlpack.hpp" // General mlpack include for NaiveBayesClassifier
#include <iostream> // For error messages

NaiveBayesModelOperation::NaiveBayesModelOperation()
    : _model(std::make_unique<mlpack::NaiveBayesClassifier<>>())
{}

std::string NaiveBayesModelOperation::getName() const {
    return "Naive Bayes";
}

std::unique_ptr<MLModelParametersBase> NaiveBayesModelOperation::getDefaultParameters() const {
    return std::make_unique<NaiveBayesParameters>();
}

bool NaiveBayesModelOperation::train(arma::Mat<double> const& features,
                                   arma::Row<size_t> const& labels,
                                   MLModelParametersBase const* params_base) {
    if (!_model) {
        std::cerr << "NaiveBayesModelOperation error: Model not initialized." << std::endl;
        return false;
    }

    // const NaiveBayesParameters* nbParams = dynamic_cast<const NaiveBayesParameters*>(params_base);
    // mlpack's NaiveBayesClassifier::Train primarily uses features, labels, and number of classes.
    // Specific parameters like epsilon from NaiveBayesParameters might be used if the mlpack model supports it
    // or if a custom training loop was implemented. For standard mlpack NBC, it's not directly used in Train().

    if (labels.empty()) {
        std::cerr << "NaiveBayesModelOperation::train: Labels cannot be empty." << std::endl;
        return false;
    }
    _numClasses = arma::max(labels) + 1;
    if (_numClasses < 2) {
        // Mlpack NaiveBayesClassifier requires at least 2 classes.
        // If training data only contains one class, this will be an issue.
        // A common workaround is to set numClasses to 2 if it's 1, but this has implications.
        std::cout << "NaiveBayesModelOperation::train: Warning - only one class detected in labels. Mlpack requires numClasses >= 2. Setting numClasses to 2." << std::endl;
        _numClasses = 2;
    }

    try {
        // For NaiveBayesParameters, if epsilon was to be used for variance smoothing, 
        // it would typically be a parameter to the NaiveBayesClassifier constructor or a setter method, 
        // neither of which is standard for the mlpack version's Train() method directly.
        // We proceed assuming the default behavior of mlpack's NBC.
        _model->Train(features, labels, _numClasses);
    } catch (const std::exception& e) {
        std::cerr << "NaiveBayesModelOperation::train failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool NaiveBayesModelOperation::predict(arma::Mat<double> const& features,
                                     arma::Row<size_t>& predictions) {
    if (!_model) {
        std::cerr << "NaiveBayesModelOperation error: Model not trained or initialized." << std::endl;
        return false;
    }
    if (features.empty()) {
        std::cerr << "NaiveBayesModelOperation::predict: Input features are empty." << std::endl;
        // predictions will remain empty or as is. 
        return false; // Or true if empty prediction for empty features is valid.
    }
    try {
        _model->Classify(features, predictions);
    } catch (const std::exception& e) {
        std::cerr << "NaiveBayesModelOperation::predict failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool NaiveBayesModelOperation::predictProbabilities(arma::Mat<double> const& features,
                                                  arma::Row<size_t>& predictions,
                                                  arma::Mat<double>& probabilities) {
    if (!_model || _numClasses == 0) {
         std::cerr << "NaiveBayesModelOperation error: Model not trained or numClasses not set for probability prediction." << std::endl;
        return false;
    }
    if (features.empty()) {
        std::cerr << "NaiveBayesModelOperation::predictProbabilities: Input features are empty." << std::endl;
        probabilities.clear();
        return false; 
    }
    try {
        // For Naive Bayes, Classify can also return probabilities
        // The probabilities matrix should have _numClasses rows and features.n_cols columns.
        _model->Classify(features, predictions, probabilities);
    } catch (const std::exception& e) {
        std::cerr << "NaiveBayesModelOperation::predictProbabilities failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

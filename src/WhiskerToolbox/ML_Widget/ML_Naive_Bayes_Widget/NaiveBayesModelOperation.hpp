#ifndef NAIVEBAYESMODELOPERATION_HPP
#define NAIVEBAYESMODELOPERATION_HPP

#include "ML_Widget/MLModelOperation.hpp"

#include "mlpack.hpp"

#include <memory>

class NaiveBayesModelOperation : public MLModelOperation {
public:
    NaiveBayesModelOperation();
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getDefaultParameters() const override;
    bool train(arma::Mat<double> const & features, arma::Row<size_t> const & labels, MLModelParametersBase const* params) override;
    bool predict(arma::Mat<double> const & features, arma::Row<size_t> & predictions) override;
    bool predictProbabilities(arma::Mat<double> const & features,
                              arma::Row<size_t> & predictions,
                              arma::Mat<double> & probabilities) override;
private:
    std::unique_ptr<mlpack::NaiveBayesClassifier<>> _model;
    size_t _numClasses = 0; // Required for NaiveBayesClassifier training
};



#endif// NAIVEBAYESMODELOPERATION_HPP

#ifndef WHISKERTOOLBOX_MLMODELOPERATION_HPP
#define WHISKERTOOLBOX_MLMODELOPERATION_HPP

#include "MLModelParameters.hpp"
#include "DataManager/DataManagerTypes.hpp" // For DataTypeVariant
#include "mlpack/core.hpp" // For arma::Mat and arma::Row

#include <string>
#include <memory>
#include <vector>

class MLModelOperation {
public:
    virtual ~MLModelOperation() = default;
    [[nodiscard]] virtual std::string getName() const = 0;
    [[nodiscard]] virtual std::unique_ptr<MLModelParametersBase> getDefaultParameters() const = 0;

    // Trains the model
    virtual bool train(arma::Mat<double> const& features, 
                       arma::Row<size_t> const& labels, 
                       MLModelParametersBase const* params) = 0;

    // Predicts labels for new features
    virtual bool predict(arma::Mat<double> const& features, 
                         arma::Row<size_t>& predictions) = 0;
    
    virtual bool predictProbabilities(arma::Mat<double> const& features,
                                      arma::Row<size_t> & predictions,
                                      arma::Mat<double>& probabilities) {
        probabilities.clear();
        return false; 
    }

    // Returns the underlying model as a variant or void* if needed for specific interactions
    // For now, let's assume direct interaction is not needed outside this class.
    // virtual DataTypeVariant getModel() = 0; 
};



#endif //WHISKERTOOLBOX_MLMODELOPERATION_HPP 

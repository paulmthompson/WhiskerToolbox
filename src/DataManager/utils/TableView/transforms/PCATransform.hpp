#ifndef PCA_TRANSFORM_HPP
#define PCA_TRANSFORM_HPP

#include "ITableTransform.hpp"
#include "utils/TableView/interfaces/IMultiColumnComputer.h"

#include <armadillo>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief PCA configuration.
 */
struct PCAConfig {
    bool center = true;                 // mean-center features
    bool standardize = false;           // if true, z-score per feature
    std::vector<std::string> include;   // explicit include list (optional)
    std::vector<std::string> exclude;   // explicit exclude list (optional)
};

/**
 * @brief Multi-output computer that returns principal component scores as separate columns.
 */
class PCAMultiColumnComputer : public IMultiColumnComputer<double> {
public:
    PCAMultiColumnComputer(arma::mat X_keptRows_by_features,
                           bool center,
                           bool standardize)
        : m_X(std::move(X_keptRows_by_features))
        , m_center(center)
        , m_standardize(standardize) {}

    [[nodiscard]] std::pair<std::vector<std::vector<double>>, ColumnEntityIds> computeBatch(ExecutionPlan const & plan) const override;
    [[nodiscard]] auto getOutputNames() const -> std::vector<std::string> override;
    [[nodiscard]] auto getDependencies() const -> std::vector<std::string> override { return {}; }
    [[nodiscard]] auto getSourceDependency() const -> std::string override { return "__derived__"; }

private:
    struct FitCache {
        arma::mat scores;               // rows x components
        arma::vec explained;            // variance explained per component (fraction 0..1)
        std::vector<std::string> names; // PC names with variance explained
    };

    void fitIfNeeded() const;

    arma::mat m_X; // rows kept after filtering, by features
    bool m_center;
    bool m_standardize;

    mutable std::optional<FitCache> m_cache;
};

/**
 * @brief Table-to-table PCA transform.
 */
class PCATransform : public ITableTransform {
public:
    explicit PCATransform(PCAConfig config) : m_config(std::move(config)) {}

    [[nodiscard]] auto name() const -> std::string override { return "PCA"; }
    [[nodiscard]] auto apply(TableView const & source) -> TableView override;

private:
    [[nodiscard]] auto selectNumericColumns(TableView const & source) const -> std::vector<std::string>;
    [[nodiscard]] static auto extractMatrixAndKeptRows(TableView const & source,
                                                       std::vector<std::string> const & featureColumns,
                                                       bool dropNaNInf)
        -> std::pair<arma::mat, std::vector<size_t>>;

    PCAConfig m_config;
};

#endif // PCA_TRANSFORM_HPP



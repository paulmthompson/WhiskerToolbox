#include "PCATransform.hpp"

#include "utils/TableView/columns/Column.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/interfaces/MultiComputerOutputView.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

using std::size_t;

// Utility to check numeric type by type_index against supported scalar types
static bool isNumericType(std::type_index ti) {
    return ti == std::type_index(typeid(double)) ||
           ti == std::type_index(typeid(float)) ||
           ti == std::type_index(typeid(int)) ||
           ti == std::type_index(typeid(int64_t));
}

void PCAMultiColumnComputer::fitIfNeeded() const {
    if (m_cache.has_value()) return;

    arma::mat X = m_X;// copy local to allow centering/standardizing

    // Center / standardize
    arma::rowvec mean, stddev;
    if (m_center || m_standardize) {
        mean = arma::mean(X, 0);
        X.each_row() -= mean;
    }
    if (m_standardize) {
        stddev = arma::stddev(X, 0, 0);// normalize by N-1 (default unbiased=0)
        for (arma::uword j = 0; j < X.n_cols; ++j) {
            double s = stddev(j);
            if (s > 0) X.col(j) /= s;// avoid divide by zero; constant columns become zeros
        }
    }

    if (X.n_rows == 0 || X.n_cols == 0) {
        // No valid data
        FitCache empty;
        empty.scores.set_size(0, 0);
        empty.explained.set_size(0);
        empty.names.clear();
        m_cache = std::move(empty);
        return;
    }

    // PCA via SVD: X = U S V^T; columns of V are PCs; scores = X * V
    arma::mat U, V;
    arma::vec s;
    bool ok = arma::svd_econ(U, s, V, X, "both");
    if (!ok) {
        throw std::runtime_error("PCAMultiColumnComputer: SVD failed");
    }

    // Explained variance proportions
    arma::vec var = arma::square(s) / static_cast<double>(X.n_rows - (m_center ? 1 : 0));
    double const total_var = arma::accu(var);
    arma::vec explained;
    if (total_var > 0.0) {
        explained = var / total_var;
    } else {
        explained = arma::vec(var.n_elem, arma::fill::zeros);
    }

    // Scores = X * V (rows x comps)
    arma::mat scores = X * V;

    // Debug: Print scores matrix dimensions
    std::cerr << "DEBUG PCA fit: scores matrix dimensions: " << scores.n_rows << " x " << scores.n_cols << std::endl;
    std::cerr << "DEBUG PCA fit: V matrix dimensions: " << V.n_rows << " x " << V.n_cols << std::endl;
    std::cerr << "DEBUG PCA fit: s vector size: " << s.n_elem << std::endl;

    // Names with variance explained
    std::vector<std::string> names;
    names.reserve(V.n_cols);
    for (arma::uword j = 0; j < V.n_cols; ++j) {
        double const pct = explained(j) * 100.0;
        std::ostringstream os;
        os.setf(std::ios::fixed);
        os.precision(1);
        os << "PC" << (j + 1) << " (" << pct << "%)";
        names.emplace_back(os.str());
    }

    FitCache cache;
    cache.scores = std::move(scores);
    cache.explained = std::move(explained);
    cache.names = std::move(names);
    m_cache = std::move(cache);
}

auto PCAMultiColumnComputer::computeBatch(ExecutionPlan const & /*plan*/) const -> std::vector<std::vector<double>> {
    fitIfNeeded();
    std::vector<std::vector<double>> outputs;
    if (!m_cache.has_value()) return outputs;

    arma::mat const & S = m_cache->scores;
    std::cerr << "DEBUG PCA computeBatch: scores matrix dimensions: " << S.n_rows << " x " << S.n_cols << std::endl;

    outputs.resize(S.n_cols);
    for (arma::uword j = 0; j < S.n_cols; ++j) {
        outputs[j].resize(S.n_rows);
        for (arma::uword i = 0; i < S.n_rows; ++i) {
            outputs[j][i] = S(i, j);
        }
    }

    std::cerr << "DEBUG PCA computeBatch: returning " << outputs.size() << " columns with " << (outputs.empty() ? 0 : outputs[0].size()) << " rows each" << std::endl;
    return outputs;
}

auto PCAMultiColumnComputer::getOutputNames() const -> std::vector<std::string> {
    fitIfNeeded();
    if (m_cache && !m_cache->names.empty()) return m_cache->names;
    // Fallback naming if unfitted/empty
    std::vector<std::string> names;
    names.reserve(m_X.n_cols);
    for (size_t i = 0; i < m_X.n_cols; ++i) {
        names.emplace_back("PC" + std::to_string(i + 1));
    }
    return names;
}

auto PCATransform::selectNumericColumns(TableView const & source) const -> std::vector<std::string> {
    auto names = source.getColumnNames();
    std::vector<std::string> selected;
    selected.reserve(names.size());

    std::set<std::string> includeSet(m_config.include.begin(), m_config.include.end());
    std::set<std::string> excludeSet(m_config.exclude.begin(), m_config.exclude.end());

    for (auto const & n: names) {
        if (!m_config.include.empty() && includeSet.find(n) == includeSet.end()) continue;
        if (!m_config.exclude.empty() && excludeSet.find(n) != excludeSet.end()) continue;
        auto ti = source.getColumnTypeIndex(n);
        if (isNumericType(ti)) {
            selected.push_back(n);
        }
    }

    if (!m_config.include.empty()) {
        // Validate all included are numeric
        for (auto const & n: m_config.include) {
            if (excludeSet.count(n)) continue;
            if (!source.hasColumn(n)) {
                throw std::runtime_error("PCATransform: Included column does not exist: " + n);
            }
            if (!isNumericType(source.getColumnTypeIndex(n))) {
                throw std::runtime_error("PCATransform: Included column is not numeric: " + n);
            }
        }
    }

    if (selected.empty()) {
        throw std::runtime_error("PCATransform: No numeric columns available for PCA");
    }
    return selected;
}

auto PCATransform::apply(TableView const & source) -> TableView {
    // Select numeric feature columns
    auto features = selectNumericColumns(source);

    // Extract matrix and kept rows first (drop NaN/Inf rows per config)
    auto [X, kept] = extractMatrixAndKeptRows(source, features, true);

    // Debug: Print kept vector information
    std::cerr << "DEBUG PCA: Original rows: " << source.getRowCount() << std::endl;
    std::cerr << "DEBUG PCA: Kept rows count: " << kept.size() << std::endl;
    std::cerr << "DEBUG PCA: Kept indices: ";
    for (size_t idx: kept) {
        std::cerr << idx << " ";
    }
    std::cerr << std::endl;
    std::cerr << "DEBUG PCA: X matrix dimensions: " << X.n_rows << " x " << X.n_cols << std::endl;

    // Prepare PCA multi-computer with X (kept rows)
    auto pcaComputer = std::make_unique<PCAMultiColumnComputer>(std::move(X), m_config.center, m_config.standardize);

    TableViewBuilder builder(source.getDataManagerExtension());
    // PCA outputs are derived (no expansion-capable sources). Build rows as a simple
    // index space matching the kept rows to ensure 1:1 alignment with preserved EntityIds.
    {
        std::vector<size_t> indices;
        indices.resize(kept.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            indices[i] = i;
        }
        builder.setRowSelector(std::make_unique<IndexSelector>(std::move(indices)));
    }

    // Add PCA components as columns via multi-output view; base name empty because names include variance
    builder.addColumns<double>("", std::move(pcaComputer));

    // Build the table first
    auto transformed_table = builder.build();

    auto entity_ids = source.getEntityIds();
    transformed_table.setDirectEntityIds(std::move(entity_ids));

    return transformed_table;
}

auto PCATransform::extractMatrixAndKeptRows(TableView const & source,
                                            std::vector<std::string> const & featureColumns,
                                            bool dropNaNInf)
        -> std::pair<arma::mat, std::vector<size_t>> {
    size_t const nrows = source.getRowCount();
    std::vector<std::vector<double>> cols;
    cols.reserve(featureColumns.size());
    // Need non-const access to source for materialization
    auto & nonConstSource = const_cast<TableView &>(source);
    for (auto const & name: featureColumns) {
        auto const ti = source.getColumnTypeIndex(name);
        if (ti == std::type_index(typeid(double))) {
            cols.push_back(nonConstSource.getColumnValues<double>(name));
        } else if (ti == std::type_index(typeid(float))) {
            auto const & v = nonConstSource.getColumnValues<float>(name);
            std::vector<double> d;
            d.reserve(v.size());
            for (float x: v) d.push_back(static_cast<double>(x));
            cols.emplace_back(std::move(d));
        } else if (ti == std::type_index(typeid(int))) {
            auto const & v = nonConstSource.getColumnValues<int>(name);
            std::vector<double> d;
            d.reserve(v.size());
            for (int x: v) d.push_back(static_cast<double>(x));
            cols.emplace_back(std::move(d));
        } else if (ti == std::type_index(typeid(int64_t))) {
            auto const & v = nonConstSource.getColumnValues<int64_t>(name);
            std::vector<double> d;
            d.reserve(v.size());
            for (int64_t x: v) d.push_back(static_cast<double>(x));
            cols.emplace_back(std::move(d));
        } else {
            throw std::runtime_error("PCATransform: Non-numeric column encountered: " + name);
        }
        if (cols.back().size() != nrows) {
            throw std::runtime_error("PCATransform: Column row count mismatch");
        }
    }

    std::vector<size_t> kept;
    kept.reserve(nrows);
    for (size_t r = 0; r < nrows; ++r) {
        bool ok = true;
        if (dropNaNInf) {
            for (auto const & c: cols) {
                double v = c[r];
                if (!std::isfinite(v)) {
                    ok = false;
                    break;
                }
            }
        }
        if (ok) kept.push_back(r);
    }

    arma::mat X(kept.size(), cols.size());
    for (size_t j = 0; j < cols.size(); ++j) {
        for (size_t i = 0; i < kept.size(); ++i) {
            X(i, j) = cols[j][kept[i]];
        }
    }
    return {std::move(X), std::move(kept)};
}

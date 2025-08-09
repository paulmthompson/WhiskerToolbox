#ifndef DATAVIEW_SORTBYCOLUMNTRANSFORM_HPP
#define DATAVIEW_SORTBYCOLUMNTRANSFORM_HPP

#include "Analysis_Dashboard/DataView/Transforms/IDataViewTransform.hpp"
#include "Analysis_Dashboard/DataView/Transforms/ColumnAccessor.hpp"

#include <algorithm>
#include <cmath>

class SortByColumnTransform final : public IDataViewTransform {
public:
    enum class Order { Asc, Desc };

    QString tableIdPrimary;
    QString columnPrimary;
    std::optional<QString> tableIdSecondary;
    std::optional<QString> columnSecondary;
    Order order{Order::Asc};

    QString id() const override { return "SortByColumn"; }
    QString displayName() const override { return "Sort By Column"; }

    bool apply(DataViewContext const & context, DataViewState & state) override {
        if (tableIdPrimary.isEmpty() || columnPrimary.isEmpty()) return true; // nothing to do

        auto primaryVals = DataViewColumns::loadScalarAsDoubles(context.tableRegistry,
                                                                 tableIdPrimary, columnPrimary,
                                                                 context.rowCount);
        if (!primaryVals || primaryVals->size() != context.rowCount) {
            return false; // invalid configuration; skip
        }

        std::optional<std::vector<double>> secondaryVals;
        if (tableIdSecondary && columnSecondary) {
            secondaryVals = DataViewColumns::loadScalarAsDoubles(context.tableRegistry,
                                                                 *tableIdSecondary,
                                                                 *columnSecondary,
                                                                 context.rowCount);
            if (secondaryVals && secondaryVals->size() != context.rowCount) secondaryVals.reset();
        }

        auto & orderVec = state.rowOrder;
        auto const & mask = state.rowMask;
        bool asc = (order == Order::Asc);

        auto is_nan = [](double v){ return std::isnan(v); };
        auto cmpVal = [&](double a, double b){
            // NaN bottom regardless of order
            bool na = is_nan(a), nb = is_nan(b);
            if (na != nb) return !na && nb; // a before b if a is not NaN
            if (a == b) return false;
            return asc ? (a < b) : (a > b);
        };

        // Stable sort only among kept rows; keep dropped rows in place
        std::stable_sort(orderVec.begin(), orderVec.end(), [&](size_t i, size_t j){
            if (!mask[i] && !mask[j]) return false; // keep dropped relative order
            if (!mask[i]) return false;             // dropped after kept
            if (!mask[j]) return true;
            double ai = (*primaryVals)[i];
            double aj = (*primaryVals)[j];
            if (ai != aj || (is_nan(ai) != is_nan(aj))) return cmpVal(ai, aj);
            if (secondaryVals) {
                double bi = (*secondaryVals)[i];
                double bj = (*secondaryVals)[j];
                if (bi != bj || (is_nan(bi) != is_nan(bj))) return cmpVal(bi, bj);
            }
            return false; // stable
        });

        return true;
    }
};

#endif // DATAVIEW_SORTBYCOLUMNTRANSFORM_HPP


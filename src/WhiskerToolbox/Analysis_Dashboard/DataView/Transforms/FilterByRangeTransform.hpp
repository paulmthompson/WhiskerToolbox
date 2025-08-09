#ifndef DATAVIEW_FILTERBYRANGETRANSFORM_HPP
#define DATAVIEW_FILTERBYRANGETRANSFORM_HPP

#include "Analysis_Dashboard/DataView/Transforms/IDataViewTransform.hpp"
#include "Analysis_Dashboard/DataView/Transforms/ColumnAccessor.hpp"

#include <algorithm>

class FilterByRangeTransform final : public IDataViewTransform {
public:
    enum class Comparator { LT, LE, GT, GE, EQ, NE };

    QString tableId;
    QString columnName;
    Comparator comparator{Comparator::GT};
    double value{0.0};

    QString id() const override { return "FilterByRange"; }
    QString displayName() const override { return "Filter By Range"; }

    bool apply(DataViewContext const & context, DataViewState & state) override {
        if (tableId.isEmpty() || columnName.isEmpty()) return true;
        auto vals = DataViewColumns::loadScalarAsDoubles(context.tableRegistry, tableId, columnName, context.rowCount);
        if (!vals || vals->size() != context.rowCount) return false;
        auto & mask = state.rowMask;
        for (size_t i = 0; i < vals->size(); ++i) {
            if (!mask[i]) continue;
            double x = (*vals)[i];
            bool keep = true;
            switch (comparator) {
                case Comparator::LT: keep = x < value; break;
                case Comparator::LE: keep = x <= value; break;
                case Comparator::GT: keep = x > value; break;
                case Comparator::GE: keep = x >= value; break;
                case Comparator::EQ: keep = x == value; break;
                case Comparator::NE: keep = x != value; break;
            }
            mask[i] = keep ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0);
        }
        return true;
    }
};

#endif // DATAVIEW_FILTERBYRANGETRANSFORM_HPP


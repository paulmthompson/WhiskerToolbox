#include "Column.h"
#include "utils/TableView/core/TableView.h"

#include <stdexcept>


// Explicit instantiation for commonly used types
template class Column<double>;
template class Column<bool>;
template class Column<int>;
template class Column<std::vector<TimeFrameIndex>>;

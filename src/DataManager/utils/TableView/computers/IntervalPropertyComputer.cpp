#include "IntervalPropertyComputer.h"

// Template specializations for different data types
template std::pair<std::vector<int64_t>, ColumnEntityIds> 
IntervalPropertyComputer<int64_t>::compute(ExecutionPlan const & plan) const;

template std::pair<std::vector<float>, ColumnEntityIds> 
IntervalPropertyComputer<float>::compute(ExecutionPlan const & plan) const;

template std::pair<std::vector<double>, ColumnEntityIds> 
IntervalPropertyComputer<double>::compute(ExecutionPlan const & plan) const;
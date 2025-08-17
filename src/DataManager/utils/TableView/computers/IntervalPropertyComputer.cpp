#include "IntervalPropertyComputer.h"

// Template specializations for different data types
template std::vector<int64_t> IntervalPropertyComputer<int64_t>::compute(ExecutionPlan const & plan) const;

template std::vector<float> IntervalPropertyComputer<float>::compute(ExecutionPlan const & plan) const;

template std::vector<double> IntervalPropertyComputer<double>::compute(ExecutionPlan const & plan) const;
#ifndef FEATURE_EXTRACTION_FEATURE_EXTRACTOR_HPP
#define FEATURE_EXTRACTION_FEATURE_EXTRACTOR_HPP

#include "FeatureVector.hpp"
#include "IFeature.hpp"


class FeatureExtractor {
public:
    template<typename T_in, typename... TStrategies>
    auto extract(T_in const & item, TStrategies &... strategies) const {

        // Use `if constexpr` to check the number of strategies at compile time.
        if constexpr (sizeof...(strategies) == 1) {
            // COMPILE-TIME BRANCH: Only one strategy was passed.
            // The return type of the function will be the return type of this single extract() call.

            // A fold expression is used to unpack the single strategy from the parameter pack.
            return (strategies.extract(item), ...);
        } else {
            // COMPILE-TIME BRANCH: Multiple (or zero) strategies were passed.
            // The return type of the function will be FeatureVector.

            FeatureVector features;
            // A fold expression unpacks all strategies and calls append() on each result.
            (features.append(strategies.extract(item)), ...);
            return features;
        }
    }
};

// --- Example Usage ---
/*
void example() {
    Line2D my_line = ...;
    LineLengthStrategy length_strategy;
    LineCentroidStrategy centroid_strategy;
    
    FeatureExtractor extractor;

    // The compiler generates a version of `extract` that returns a `double`.
    double length = extractor.extract(my_line, length_strategy);

    // The compiler generates a different version of `extract` that returns a `FeatureVector`.
    FeatureVector multi_features = extractor.extract(my_line, length_strategy, centroid_strategy);
}
*/

#endif// FEATURE_EXTRACTION_FEATURE_EXTRACTOR_HPP
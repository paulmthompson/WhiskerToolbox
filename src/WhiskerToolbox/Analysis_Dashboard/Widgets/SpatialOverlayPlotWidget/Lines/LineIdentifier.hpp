#ifndef LINE_IDENTIFIER_HPP
#define LINE_IDENTIFIER_HPP

#include <cstdint>

/**
 * @brief Unique identifier for a line in the collapsed view
 */
struct LineIdentifier {
    int64_t time_frame {0};
    int line_id {0};

    bool operator==(LineIdentifier const & other) const {
        return time_frame == other.time_frame && line_id == other.line_id;
    }
};

// Hash function for LineIdentifier

template<>
struct std::hash<LineIdentifier> {
    std::size_t operator()(LineIdentifier const & id) const {
        return std::hash<int64_t>{}(id.time_frame) ^
               (std::hash<int>{}(id.line_id) << 1);
    }
};

#endif// LINE_IDENTIFIER_HPP
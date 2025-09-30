#ifndef HUNGARIAN_HPP
#define HUNGARIAN_HPP

#include <type_traits>
#include <vector>

namespace Munkres {

/**
 * @brief Hungarian algorithm (Munkres algorithm) implementation
 * 
 * Solves the assignment problem to find minimum cost assignment in a bipartite graph.
 * 
 * @tparam Container Container type (e.g., std::vector)
 * @tparam T Element type (must be integral)
 * @tparam Args Additional template parameters for the container
 * @param original Cost matrix where original[i][j] is the cost of assigning worker i to job j
 * @param allow_negatives Whether to allow negative costs in the matrix
 * @return Total minimum cost of the optimal assignment
 * 
 * @note After the algorithm completes, the original matrix is modified:
 *       - Zeros indicate the optimal assignment
 *       - Use the starred zeros to extract the actual assignments
 */
template<template<typename, typename...> class Container,
         typename T,
         typename... Args>
typename std::enable_if<std::is_integral<T>::value, T>::type
hungarian(Container<Container<T, Args...>> const & original, bool allow_negatives = true);

/**
 * @brief Hungarian algorithm with assignment matrix output
 * 
 * Solves the assignment problem and returns both cost and assignment matrix.
 * 
 * @param original Cost matrix where original[i][j] is the cost of assigning worker i to job j
 * @param assignment_matrix Output matrix where assignment_matrix[i][j] = 1 if worker i is assigned to job j
 * @param allow_negatives Whether to allow negative costs in the matrix
 * @return Total minimum cost of the optimal assignment
 */
template<template<typename, typename...> class Container,
         typename T,
         typename... Args>
typename std::enable_if<std::is_integral<T>::value, T>::type
hungarian_with_assignment(Container<Container<T, Args...>> const & original,
                          std::vector<std::vector<int>> & assignment_matrix,
                          bool allow_negatives = true);

// Non-template overloads for the common std::vector<std::vector<int>> case.
// MSVC has difficulty deducing nested template-template parameters in some
// call sites; these overloads forward to the template with explicit
// template arguments to avoid that problem.
inline int hungarian(std::vector<std::vector<int>> const & original, bool allow_negatives = true) {
    return hungarian<std::vector, int>(original, allow_negatives);
}

inline int hungarian_with_assignment(std::vector<std::vector<int>> const & original,
                                     std::vector<std::vector<int>> & assignment_matrix,
                                     bool allow_negatives = true) {
    return hungarian_with_assignment<std::vector, int>(original, assignment_matrix, allow_negatives);
}

}// namespace Munkres

#endif// HUNGARIAN_HPP
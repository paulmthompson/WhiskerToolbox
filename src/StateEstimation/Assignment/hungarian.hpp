#ifndef HUNGARIAN_HPP
#define HUNGARIAN_HPP

#include <vector>
#include <type_traits>

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
template<template <typename, typename...> class Container,
         typename T,
         typename... Args>
typename std::enable_if<std::is_integral<T>::value, T>::type
hungarian(const Container<Container<T,Args...>>& original, bool allow_negatives = true);

} // namespace Munkres

#endif // HUNGARIAN_HPP
#ifndef METAPROGRAMMING_UTILS_HPP
#define METAPROGRAMMING_UTILS_HPP

#include <type_traits>
#include <tuple>
#include <variant>

// ===================================================================
// == Metaprogramming Utilities 
// ===================================================================

// Helper to convert a tuple of types into a variant of those types

template <typename T>
struct tuple_to_variant;

template <typename... Types>
struct tuple_to_variant<std::tuple<Types...>> {
    using type = std::variant<Types...>;
};


template <typename T>
using tuple_to_variant_t = typename tuple_to_variant<T>::type;


// Helper to check if a type T is present in a tuple

template <typename T, typename Tuple>
struct is_in_tuple;

template <typename T, typename... Us>
struct is_in_tuple<T, std::tuple<Us...>> : std::disjunction<std::is_same<T, Us>...> {};

template <typename T, typename Tuple>
inline constexpr bool is_in_tuple_v = is_in_tuple<T, Tuple>::value;

// Helper to apply a function (like a lambda) to each type in a tuple
template<typename F, typename... Ts>
constexpr void for_each_type_impl(F&& func, std::tuple<Ts...>) {
    // Use fold expression to call function for each type
    (func(std::integral_constant<int, 0>{}, Ts{}), ...);
}

template<typename Tuple, typename F>
constexpr void for_each_type(F&& func) {
    for_each_type_impl(std::forward<F>(func), Tuple{});
}

#endif // METAPROGRAMMING_UTILS_HPP
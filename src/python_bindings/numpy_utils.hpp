#pragma once

/**
 * @file numpy_utils.hpp
 * @brief Zero-copy helpers for C++ span ↔ NumPy array conversion.
 *
 * These utilities require NumPy to be available at **runtime** (for the
 * embedded Python interpreter).  They compile against pybind11's numpy
 * header alone — no NumPy C headers required at build time.
 */

#include <pybind11/numpy.h>

#include <span>
#include <vector>

namespace py = pybind11;

namespace wt::python {

/**
 * @brief Create a **read-only** NumPy array that views a C++ span (zero-copy).
 *
 * @tparam T  Element type (float, double, int64_t, …).
 * @param data   Contiguous span of const data.
 * @param owner  A Python handle whose ref-count keeps the underlying C++
 *               memory alive.  Typically `py::cast(shared_ptr)`.
 * @return A 1-D NumPy array with the WRITEABLE flag cleared.
 */
template<typename T>
py::array_t<T> span_to_numpy_readonly(std::span<T const> data, py::handle owner) {
    if (data.empty()) {
        return py::array_t<T>(0);
    }
    auto result = py::array_t<T>(
        {static_cast<py::ssize_t>(data.size())},   // shape
        {static_cast<py::ssize_t>(sizeof(T))},      // strides
        data.data(),                                  // data pointer
        owner                                         // prevent GC of C++ object
    );
    // Mark the array as read-only to prevent accidental mutation
    py::detail::array_proxy(result.ptr())->flags
        &= ~py::detail::npy_api::NPY_ARRAY_WRITEABLE_;
    return result;
}

/**
 * @brief Copy a NumPy array into a std::vector.
 */
template<typename T>
std::vector<T> numpy_to_vector(
    py::array_t<T, py::array::c_style | py::array::forcecast> arr) {
    auto buf = arr.request();
    auto * ptr = static_cast<T *>(buf.ptr);
    return std::vector<T>(ptr, ptr + buf.size);
}

} // namespace wt::python

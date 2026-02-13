#ifndef OUTPUT_REDIRECTOR_HPP
#define OUTPUT_REDIRECTOR_HPP

#include <pybind11/pybind11.h>

#include <string>

/**
 * @brief C++ class exposed to Python to capture sys.stdout / sys.stderr.
 *
 * An instance is injected as sys.stdout and sys.stderr inside the embedded
 * interpreter.  Every `write()` call appends to an internal buffer that can
 * be drained by C++ after execution completes.
 *
 * The class satisfies the minimal Python "file-like object" protocol
 * (write, flush, readable, writable, seekable) so that the Python runtime
 * and libraries like `traceback` accept it without complaint.
 */
class OutputRedirector {
public:
    OutputRedirector() = default;

    /// Called by Python's `print()` / `sys.stdout.write()`
    void write(std::string const & text) { _buffer += text; }

    /// Called by Python after print() — we buffer, so this is a no-op.
    void flush() {} // no-op: we buffer everything

    /// Return and clear the accumulated text.
    [[nodiscard]] std::string drain() {
        std::string out;
        std::swap(out, _buffer);
        return out;
    }

    /// Python "file-like" protocol helpers.
    [[nodiscard]] static bool readable() { return false; }
    [[nodiscard]] static bool writable() { return true; }
    [[nodiscard]] static bool seekable() { return false; }

private:
    std::string _buffer;
};

/**
 * @brief Register the OutputRedirector type inside a pybind11 module.
 *
 * Must be called during PYBIND11_EMBEDDED_MODULE initialisation so
 * that the type is available before the interpreter tries to redirect
 * stdout/stderr.
 */
inline void bind_output_redirector(pybind11::module_ & m) {
    namespace py = pybind11;

    py::class_<OutputRedirector>(m, "OutputRedirector")
        .def(py::init<>())
        .def("write", &OutputRedirector::write)
        .def("flush", &OutputRedirector::flush)
        .def("readable", &OutputRedirector::readable)
        .def("writable", &OutputRedirector::writable)
        .def("seekable", &OutputRedirector::seekable);
}

#endif // OUTPUT_REDIRECTOR_HPP

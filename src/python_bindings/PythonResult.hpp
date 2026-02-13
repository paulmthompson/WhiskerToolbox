#ifndef PYTHON_RESULT_HPP
#define PYTHON_RESULT_HPP

#include <string>

/**
 * @brief Result of a Python code execution.
 *
 * Contains captured stdout/stderr text and a success flag.
 */
struct PythonResult {
    std::string stdout_text;
    std::string stderr_text;
    bool success{false};
};

#endif // PYTHON_RESULT_HPP

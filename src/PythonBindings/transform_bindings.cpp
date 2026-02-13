#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "transforms/data_transforms.hpp"
#include "transforms/DigitalIntervalSeries/Digital_Interval_Boolean/digital_interval_boolean.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManagerTypes.hpp"

#include <memory>
#include <string>
#include <typeindex>

namespace py = pybind11;

// Python-callable transform class
class PythonTransform : public TransformOperation {
public:
    // Constructor takes a Python callable
    PythonTransform(std::string name, py::object callable) 
        : name_(std::move(name)), callable_(std::move(callable)) {}

    [[nodiscard]] std::string getName() const override {
        return name_;
    }

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override {
        return typeid(std::shared_ptr<DigitalIntervalSeries>);
    }

    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override {
        return std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(dataVariant);
    }

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters) override {
        
        // Extract the DigitalIntervalSeries from the variant
        auto const * ptr_ptr = std::get_if<std::shared_ptr<DigitalIntervalSeries>>(&dataVariant);
        if (!ptr_ptr || !(*ptr_ptr)) {
            std::cerr << "PythonTransform::execute: Invalid input data variant" << std::endl;
            return {};
        }

        try {
            // Call the Python function with the DigitalIntervalSeries
            py::gil_scoped_acquire acquire;
            py::object result = callable_(*ptr_ptr);
            
            // Try to extract the result as a DigitalIntervalSeries
            if (py::isinstance<DigitalIntervalSeries>(result)) {
                auto result_ptr = result.cast<std::shared_ptr<DigitalIntervalSeries>>();
                return result_ptr;
            } else {
                std::cerr << "PythonTransform::execute: Python function did not return a DigitalIntervalSeries" << std::endl;
                return {};
            }
        } catch (py::error_already_set &e) {
            std::cerr << "PythonTransform::execute: Python error: " << e.what() << std::endl;
            return {};
        }
    }

private:
    std::string name_;
    py::object callable_;
};

void bind_transforms(py::module_ &m) {
    // Bind the base TransformOperation class
    py::class_<TransformOperation, std::shared_ptr<TransformOperation>>(
        m, "TransformOperation", 
        "Base class for transform operations")
        .def("get_name", &TransformOperation::getName,
             "Get the name of the transform");
    // Note: canApply method uses DataTypeVariant which includes many types
    // not bound here, so we skip exposing it to Python

    // Bind BooleanParams
    py::class_<BooleanParams, TransformParametersBase>(m, "BooleanParams",
        "Parameters for boolean operations between DigitalIntervalSeries")
        .def(py::init<>())
        .def_readwrite("operation", &BooleanParams::operation, 
                      "The boolean operation to perform")
        .def_readwrite("other_series", &BooleanParams::other_series,
                      "The second series for binary operations");

    // Bind BooleanOperation enum
    py::enum_<BooleanParams::BooleanOperation>(m, "BooleanOperation",
        "Types of boolean operations")
        .value("AND", BooleanParams::BooleanOperation::AND, 
               "Intersection of intervals (both must be true)")
        .value("OR", BooleanParams::BooleanOperation::OR, 
               "Union of intervals (either can be true)")
        .value("XOR", BooleanParams::BooleanOperation::XOR, 
               "Exclusive or (exactly one must be true)")
        .value("NOT", BooleanParams::BooleanOperation::NOT, 
               "Invert the input series (ignore other_series)")
        .value("AND_NOT", BooleanParams::BooleanOperation::AND_NOT, 
               "Input AND (NOT other) - subtract other from input")
        .export_values();

    // Bind the BooleanOperation transform
    py::class_<BooleanOperation, TransformOperation, std::shared_ptr<BooleanOperation>>(
        m, "BooleanOperationTransform",
        "Transform that applies boolean logic between DigitalIntervalSeries")
        .def(py::init<>());

    // Bind the apply_boolean_operation function
    m.def("apply_boolean_operation",
          py::overload_cast<DigitalIntervalSeries const *, BooleanParams const &>(&apply_boolean_operation),
          py::arg("digital_interval_series"),
          py::arg("params"),
          "Apply a boolean operation to a DigitalIntervalSeries");

    // Allow creating custom Python transforms
    py::class_<PythonTransform, TransformOperation, std::shared_ptr<PythonTransform>>(
        m, "PythonTransform",
        "Custom transform implemented in Python")
        .def(py::init<std::string, py::object>(),
             py::arg("name"),
             py::arg("callable"),
             R"doc(
             Create a custom Python transform.
             
             Parameters:
                 name: Name of the transform
                 callable: Python function that takes a DigitalIntervalSeries and 
                          returns a DigitalIntervalSeries
             
             Example:
                 def my_transform(series):
                     # Process the series
                     new_series = DigitalIntervalSeries()
                     # ... add intervals to new_series
                     return new_series
                 
                 transform = PythonTransform("MyTransform", my_transform)
             )doc");
}

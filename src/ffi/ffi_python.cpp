#include "ffi_python.h"
#include <stdexcept>

namespace tocin {
namespace ffi {

#ifdef WITH_PYTHON
    PythonFFI::PythonFFI() : module(nullptr) {
        // Assume CompilationContext initializes Python
    }

    PythonFFI::~PythonFFI() {
        if (module) {
            Py_DECREF(module);
        }
    }

    PyObject* PythonFFI::convertToPyObject(const FFIValue& value) const {
        switch (value.getType()) {
        case FFIValueType::Int64:
            return PyLong_FromLongLong(value.getInt64());
        case FFIValueType::Double:
            return PyFloat_FromDouble(value.getDouble());
        case FFIValueType::Bool:
            return PyBool_FromLong(value.getBool() ? 1 : 0);
        case FFIValueType::String:
            return PyUnicode_FromString(value.getString().c_str());
        case FFIValueType::Null:
            Py_RETURN_NONE;
        case FFIValueType::Array: {
            auto arr = value.getArray();
            PyObject* list = PyList_New(arr.size());
            for (size_t i = 0; i < arr.size(); ++i) {
                PyList_SetItem(list, i, convertToPyObject(arr[i]));
            }
            return list;
        }
        case FFIValueType::Object: {
            auto obj = value.getObject();
            PyObject* dict = PyDict_New();
            for (const auto& [key, val] : obj) {
                PyDict_SetItemString(dict, key.c_str(), convertToPyObject(val));
            }
            return dict;
        }
        default:
            PyErr_SetString(PyExc_TypeError, "Unsupported FFI type");
            return nullptr;
        }
    }

    FFIValue PythonFFI::convertToFFIValue(PyObject* obj) const {
        if (obj == nullptr) return FFIValue();
        if (obj == Py_None) return FFIValue();
        if (PyLong_Check(obj)) return FFIValue(PyLong_AsLongLong(obj));
        if (PyFloat_Check(obj)) return FFIValue(PyFloat_AsDouble(obj));
        if (PyBool_Check(obj)) return FFIValue(obj == Py_True);
        if (PyUnicode_Check(obj)) {
            Py_ssize_t size;
            const char* str = PyUnicode_AsUTF8AndSize(obj, &size);
            return FFIValue(std::string(str, size));
        }
        if (PyList_Check(obj)) {
            FFIValue::ArrayType arr;
            Py_ssize_t size = PyList_Size(obj);
            arr.reserve(size);
            for (Py_ssize_t i = 0; i < size; ++i) {
                arr.push_back(convertToFFIValue(PyList_GetItem(obj, i)));
            }
            return FFIValue(arr);
        }
        if (PyDict_Check(obj)) {
            FFIValue::ObjectType dict;
            PyObject* key, * value;
            Py_ssize_t pos = 0;
            while (PyDict_Next(obj, &pos, &key, &value)) {
                if (PyUnicode_Check(key)) {
                    Py_ssize_t size;
                    const char* k = PyUnicode_AsUTF8AndSize(key, &size);
                    dict[std::string(k, size)] = convertToFFIValue(value);
                }
            }
            return FFIValue(dict);
        }
        return FFIValue();
    }

    FFIValue PythonFFI::call(const std::string& functionName, const std::vector<FFIValue>& args) {
        PyGILState_STATE gil = PyGILState_Ensure();
        try {
            if (!module) {
                module = PyImport_ImportModule("tocin_runtime");
                if (!module) {
                    PyErr_Print();
                    throw std::runtime_error("Failed to load tocin_runtime module");
                }
            }
            PyObject* func = PyObject_GetAttrString(module, functionName.c_str());
            if (!func || !PyCallable_Check(func)) {
                Py_XDECREF(func);
                throw std::runtime_error("Function not found: " + functionName);
            }
            PyObject* pyArgs = PyTuple_New(args.size());
            for (size_t i = 0; i < args.size(); ++i) {
                PyTuple_SetItem(pyArgs, i, convertToPyObject(args[i]));
            }
            PyObject* result = PyObject_CallObject(func, pyArgs);
            Py_DECREF(pyArgs);
            Py_DECREF(func);
            if (!result) {
                PyErr_Print();
                throw std::runtime_error("Error calling Python function: " + functionName);
            }
            FFIValue value = convertToFFIValue(result);
            Py_DECREF(result);
            PyGILState_Release(gil);
            return value;
        }
        catch (...) {
            PyGILState_Release(gil);
            throw;
        }
    }

    bool PythonFFI::hasFunction(const std::string& functionName) const {
        if (!module) return false;
        PyObject* func = PyObject_GetAttrString(module, functionName.c_str());
        bool exists = func && PyCallable_Check(func);
        Py_XDECREF(func);
        return exists;
    }
#else
    // Enhanced dummy implementations when Python is not available
    PythonFFI::PythonFFI() { /* Optionally log or set a flag for missing Python */ }
    PythonFFI::~PythonFFI() {}
    
    FFIValue PythonFFI::call(const std::string& functionName, const std::vector<FFIValue>& args) {
        errorHandler.reportError(error::ErrorCode::F001_FFI_UNAVAILABLE,
            "Python FFI support is not available (Python not found)",
            error::ErrorSeverity::ERROR);
        return FFIValue();
    }
    
    bool PythonFFI::hasFunction(const std::string& functionName) const {
        return false;
    }
#endif

} // namespace ffi
} // namespace tocin
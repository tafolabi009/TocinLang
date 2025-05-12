#pragma once
#ifndef FFI_PYTHON_H
#define FFI_PYTHON_H

#include "ffi_interface.h"
#include <string>
#include <vector>

#undef COMPILER // Prevent macro conflict with pyconfig.h
#include <Python.h>

namespace ffi {

    class PythonFFI : public FFIInterface {
    public:
        PythonFFI();
        ~PythonFFI() override;

        FFIValue call(const std::string& functionName, const std::vector<FFIValue>& args) override;
        bool hasFunction(const std::string& functionName) const override;

    private:
        PyObject* convertToPyObject(const FFIValue& value) const;
        FFIValue convertToFFIValue(PyObject* obj) const;
        PyObject* module;
    };

} // namespace ffi

#endif // FFI_PYTHON_H
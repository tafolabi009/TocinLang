#pragma once
#ifndef FFI_PYTHON_H
#define FFI_PYTHON_H

#include "ffi_interface.h"
#include "ffi_value.h"
#include <string>
#include <vector>

#ifdef WITH_PYTHON
// Undefine any conflicting macros before including Python.h
#ifdef COMPILER
#undef COMPILER
#endif

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif

#include <Python.h>
#endif

namespace tocin {
namespace ffi
{

    class PythonFFI : public FFIInterface
    {
    public:
        PythonFFI();
        ~PythonFFI() override;

        FFIValue call(const std::string &functionName, const std::vector<FFIValue> &args) override;
        bool hasFunction(const std::string &functionName) const override;

    private:
#ifdef WITH_PYTHON
        PyObject *convertToPyObject(const FFIValue &value) const;
        FFIValue convertToFFIValue(PyObject *obj) const;
        PyObject *module;
#endif
    };

} // namespace ffi
} // namespace tocin

#endif // FFI_PYTHON_H

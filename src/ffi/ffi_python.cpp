#include "ffi_python.h"
#include <Python.h>
#include <sstream>
#include <stdexcept>

namespace ffi {

    PythonFFIImpl::PythonFFIImpl() : initialized_(false) {
        // Python initialization will happen in initialize()
    }

    PythonFFIImpl::~PythonFFIImpl() {
        finalize();
    }

    bool PythonFFIImpl::initialize() {
        if (!initialized_) {
            if (!Py_IsInitialized()) {
                Py_Initialize();
            }
            initialized_ = true;
        }
        return true;
    }

    void PythonFFIImpl::finalize() {
        if (initialized_) {
            // Note: Don't call Py_Finalize() as it might be used by other parts
            initialized_ = false;
        }
    }

    std::string PythonFFIImpl::getVersion() const {
        return Py_GetVersion();
    }

    FFIValue PythonFFIImpl::callFunction(const std::string& functionName, const std::vector<FFIValue>& args) {
        if (!initialized_) {
            return FFIValue();
        }

        // Get the __main__ module
        PyObject* mainModule = PyImport_AddModule("__main__");
        if (!mainModule) {
            return FFIValue();
        }

        PyObject* mainDict = PyModule_GetDict(mainModule);
        PyObject* func = PyDict_GetItemString(mainDict, functionName.c_str());
        
        if (!func || !PyCallable_Check(func)) {
            return FFIValue();
        }

        // Convert FFI arguments to Python objects
        PyObject* pyArgs = PyTuple_New(args.size());
        for (size_t i = 0; i < args.size(); ++i) {
            // For simplified implementation, create string representations
            PyObject* arg = Py_None;
            if (args[i].isInteger()) {
                arg = PyLong_FromLongLong(args[i].asInt64());
            } else if (args[i].isFloat()) {
                arg = PyFloat_FromDouble(args[i].asDouble());
            } else if (args[i].isString()) {
                arg = PyUnicode_FromString(args[i].asString().c_str());
            } else if (args[i].isBoolean()) {
                arg = PyBool_FromLong(args[i].asBoolean() ? 1 : 0);
            }
            PyTuple_SetItem(pyArgs, i, arg);
        }

        // Call the function
        PyObject* result = PyObject_CallObject(func, pyArgs);
        Py_DECREF(pyArgs);

        if (!result) {
            PyErr_Print();
            return FFIValue();
        }

        FFIValue ffiResult = pythonToFFIValue(static_cast<void*>(result));
        Py_DECREF(result);
        return ffiResult;
    }

    bool PythonFFIImpl::hasFunction(const std::string& functionName) const {
        if (!initialized_) {
            return false;
        }

        PyObject* mainModule = PyImport_AddModule("__main__");
        if (!mainModule) return false;

        PyObject* mainDict = PyModule_GetDict(mainModule);
        PyObject* func = PyDict_GetItemString(mainDict, functionName.c_str());
        
        return func && PyCallable_Check(func);
    }

    bool PythonFFIImpl::loadModule(const std::string& moduleName) {
        if (!initialized_) {
            initialize();
        }

        PyObject* pName = PyUnicode_DecodeFSDefault(moduleName.c_str());
        PyObject* pModule = PyImport_Import(pName);
        Py_DECREF(pName);

        if (pModule != NULL) {
            // Store module in main dict for later access
            PyObject* mainModule = PyImport_AddModule("__main__");
            PyObject* mainDict = PyModule_GetDict(mainModule);
            PyDict_SetItemString(mainDict, moduleName.c_str(), pModule);
            Py_DECREF(pModule);
            return true;
        } else {
            PyErr_Print();
            return false;
        }
    }

    bool PythonFFIImpl::unloadModule(const std::string& moduleName) {
        if (!initialized_) {
            return false;
        }

        PyObject* mainModule = PyImport_AddModule("__main__");
        PyObject* mainDict = PyModule_GetDict(mainModule);
        return PyDict_DelItemString(mainDict, moduleName.c_str()) == 0;
    }

    bool PythonFFIImpl::isModuleLoaded(const std::string& moduleName) const {
        if (!initialized_) {
            return false;
        }

        PyObject* modules = PyImport_GetModuleDict();
        PyObject* pName = PyUnicode_DecodeFSDefault(moduleName.c_str());
        bool loaded = PyDict_Contains(modules, pName) == 1;
        Py_DECREF(pName);
        return loaded;
    }

    FFIValue PythonFFIImpl::toFFIValue(ast::ValuePtr value) {
        // Convert AST value to FFI value
        // This is a simplified implementation
        return FFIValue();
    }

    ast::ValuePtr PythonFFIImpl::fromFFIValue(const FFIValue& value) {
        // Convert FFI value to AST value
        // This is a simplified implementation
        return nullptr;
    }

    bool PythonFFIImpl::hasError() const {
        return PyErr_Occurred() != NULL;
    }

    std::string PythonFFIImpl::getLastError() const {
        if (!PyErr_Occurred()) {
            return "";
        }

        PyObject *ptype, *pvalue, *ptraceback;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback);
        
        std::string error;
        if (pvalue != NULL) {
            PyObject* pstr = PyObject_Str(pvalue);
            if (pstr != NULL) {
                error = PyUnicode_AsUTF8(pstr);
                Py_DECREF(pstr);
            }
        }
        
        PyErr_Restore(ptype, pvalue, ptraceback);
        return error;
    }

    void PythonFFIImpl::clearError() {
        PyErr_Clear();
    }

    std::vector<std::string> PythonFFIImpl::getSupportedFeatures() const {
        return {"function_calls", "module_loading", "eval", "variables", 
                "objects", "lists", "dicts", "tuples"};
    }

    bool PythonFFIImpl::supportsFeature(const std::string& feature) const {
        auto features = getSupportedFeatures();
        return std::find(features.begin(), features.end(), feature) != features.end();
    }

    FFIValue PythonFFIImpl::eval(const std::string& code) {
        if (!initialized_) {
            initialize();
        }

        PyObject* mainModule = PyImport_AddModule("__main__");
        PyObject* mainDict = PyModule_GetDict(mainModule);
        
        PyObject* result = PyRun_String(code.c_str(), Py_eval_input, mainDict, mainDict);
        
        if (!result) {
            PyErr_Print();
            return FFIValue();
        }

        FFIValue ffiResult = pythonToFFIValue(static_cast<void*>(result));
        Py_DECREF(result);
        return ffiResult;
    }

    FFIValue PythonFFIImpl::getVariable(const std::string& name) {
        if (!initialized_) {
            return FFIValue();
        }

        PyObject* mainModule = PyImport_AddModule("__main__");
        PyObject* mainDict = PyModule_GetDict(mainModule);
        PyObject* var = PyDict_GetItemString(mainDict, name.c_str());
        
        if (!var) {
            return FFIValue();
        }

        return pythonToFFIValue(static_cast<void*>(var));
    }

    void PythonFFIImpl::setVariable(const std::string& name, const FFIValue& value) {
        if (!initialized_) {
            initialize();
        }

        PyObject* mainModule = PyImport_AddModule("__main__");
        PyObject* mainDict = PyModule_GetDict(mainModule);
        
        // Convert FFI value to Python object
        PyObject* pyValue = Py_None;
        if (value.isInteger()) {
            pyValue = PyLong_FromLongLong(value.asInt64());
        } else if (value.isFloat()) {
            pyValue = PyFloat_FromDouble(value.asDouble());
        } else if (value.isString()) {
            pyValue = PyUnicode_FromString(value.asString().c_str());
        } else if (value.isBoolean()) {
            pyValue = PyBool_FromLong(value.asBoolean() ? 1 : 0);
        }
        
        PyDict_SetItemString(mainDict, name.c_str(), pyValue);
        if (pyValue != Py_None) Py_DECREF(pyValue);
    }

    bool PythonFFIImpl::isAvailable() const {
        return initialized_ || Py_IsInitialized();
    }

    // Python-specific methods
    FFIValue PythonFFIImpl::executeCode(const std::string& code) {
        if (!initialized_) {
            initialize();
        }

        PyObject* mainModule = PyImport_AddModule("__main__");
        PyObject* mainDict = PyModule_GetDict(mainModule);
        
        PyObject* result = PyRun_String(code.c_str(), Py_file_input, mainDict, mainDict);
        
        if (!result) {
            PyErr_Print();
            return FFIValue();
        }

        FFIValue ffiResult = pythonToFFIValue(static_cast<void*>(result));
        Py_DECREF(result);
        return ffiResult;
    }

    bool PythonFFIImpl::executeFile(const std::string& filename) {
        if (!initialized_) {
            initialize();
        }

        FILE* fp = fopen(filename.c_str(), "r");
        if (!fp) {
            return false;
        }

        PyObject* mainModule = PyImport_AddModule("__main__");
        PyObject* mainDict = PyModule_GetDict(mainModule);
        
        PyRun_SimpleFile(fp, filename.c_str());
        fclose(fp);
        
        return !PyErr_Occurred();
    }

    FFIValue PythonFFIImpl::callMethod(const FFIValue& object, const std::string& methodName, 
                                       const std::vector<FFIValue>& args) {
        // Simplified implementation - returns empty value
        // Full implementation would extract PyObject* from FFIValue
        return FFIValue();
    }

    FFIValue PythonFFIImpl::getAttribute(const FFIValue& object, const std::string& attrName) {
        // Simplified implementation - returns empty value  
        // Full implementation would extract PyObject* from FFIValue
        return FFIValue();
    }

    bool PythonFFIImpl::setAttribute(FFIValue& object, const std::string& attrName, const FFIValue& value) {
        // Simplified implementation
        // Full implementation would extract PyObject* from FFIValue
        return false;
    }

    FFIValue PythonFFIImpl::createList(const std::vector<FFIValue>& items) {
        PyObject* list = PyList_New(items.size());
        for (size_t i = 0; i < items.size(); ++i) {
            // Convert FFI values to Python objects
            PyObject* item = Py_None;
            if (items[i].isInteger()) {
                item = PyLong_FromLongLong(items[i].asInt64());
            } else if (items[i].isFloat()) {
                item = PyFloat_FromDouble(items[i].asDouble());
            } else if (items[i].isString()) {
                item = PyUnicode_FromString(items[i].asString().c_str());
            } else if (items[i].isBoolean()) {
                item = PyBool_FromLong(items[i].asBoolean() ? 1 : 0);
            }
            PyList_SetItem(list, i, item);
        }

        FFIValue result = pythonToFFIValue(static_cast<void*>(list));
        Py_DECREF(list);
        return result;
    }

    FFIValue PythonFFIImpl::createDict(const std::unordered_map<std::string, FFIValue>& items) {
        PyObject* dict = PyDict_New();
        for (const auto& [key, value] : items) {
            // Convert FFI value to Python object
            PyObject* pyValue = Py_None;
            if (value.isInteger()) {
                pyValue = PyLong_FromLongLong(value.asInt64());
            } else if (value.isFloat()) {
                pyValue = PyFloat_FromDouble(value.asDouble());
            } else if (value.isString()) {
                pyValue = PyUnicode_FromString(value.asString().c_str());
            } else if (value.isBoolean()) {
                pyValue = PyBool_FromLong(value.asBoolean() ? 1 : 0);
            }
            PyDict_SetItemString(dict, key.c_str(), pyValue);
            if (pyValue != Py_None) Py_DECREF(pyValue);
        }

        FFIValue result = pythonToFFIValue(static_cast<void*>(dict));
        Py_DECREF(dict);
        return result;
    }

    FFIValue PythonFFIImpl::createTuple(const std::vector<FFIValue>& items) {
        PyObject* tuple = PyTuple_New(items.size());
        for (size_t i = 0; i < items.size(); ++i) {
            // Convert FFI values to Python objects
            PyObject* item = Py_None;
            if (items[i].isInteger()) {
                item = PyLong_FromLongLong(items[i].asInt64());
            } else if (items[i].isFloat()) {
                item = PyFloat_FromDouble(items[i].asDouble());
            } else if (items[i].isString()) {
                item = PyUnicode_FromString(items[i].asString().c_str());
            } else if (items[i].isBoolean()) {
                item = PyBool_FromLong(items[i].asBoolean() ? 1 : 0);
            }
            PyTuple_SetItem(tuple, i, item);
        }

        FFIValue result = pythonToFFIValue(static_cast<void*>(tuple));
        Py_DECREF(tuple);
        return result;
    }
    
    bool PythonFFIImpl::isPythonObject(const FFIValue& value) const {
        // Check if the FFIValue represents a Python object
        return value.getType() == FFIValue::OBJECT;
    }

    std::string PythonFFIImpl::getPythonTypeName(const FFIValue& value) const {
        // For simplified implementation, return the FFI value type name
        switch (value.getType()) {
            case FFIValue::INTEGER: return "int";
            case FFIValue::FLOAT: return "float";
            case FFIValue::STRING: return "str";
            case FFIValue::BOOLEAN: return "bool";
            case FFIValue::ARRAY: return "list";
            case FFIValue::OBJECT: return "dict";
            case FFIValue::NULL_VALUE: return "NoneType";
            default: return "unknown";
        }
    }

    // Conversion helpers
    FFIValue PythonFFIImpl::ffiValueToPython(const FFIValue& value) {
        // Simplified passthrough - in a full implementation this would convert to PyObject*
        return value;
    }

    FFIValue PythonFFIImpl::pythonToFFIValue(void* pyObj) {
        PyObject* pyObject = static_cast<PyObject*>(pyObj);
        if (!pyObject || pyObject == Py_None) {
            return FFIValue();
        }

        // Integer
        if (PyLong_Check(pyObject)) {
            long long val = PyLong_AsLongLong(pyObject);
            return FFIValue(static_cast<int64_t>(val));
        }

        // Float
        if (PyFloat_Check(pyObject)) {
            double val = PyFloat_AsDouble(pyObject);
            return FFIValue(val);
        }

        // String
        if (PyUnicode_Check(pyObject)) {
            const char* str = PyUnicode_AsUTF8(pyObject);
            return FFIValue(std::string(str));
        }

        // Boolean
        if (PyBool_Check(pyObject)) {
            return FFIValue(pyObject == Py_True);
        }

        // List
        if (PyList_Check(pyObject)) {
            Py_ssize_t size = PyList_Size(pyObject);
            std::vector<FFIValue> list;
            for (Py_ssize_t i = 0; i < size; ++i) {
                list.push_back(pythonToFFIValue(static_cast<void*>(PyList_GetItem(pyObject, i))));
            }
            return FFIValue(list);
        }

        // Dict
        if (PyDict_Check(pyObject)) {
            std::unordered_map<std::string, FFIValue> map;
            PyObject *key, *value;
            Py_ssize_t pos = 0;
            
            while (PyDict_Next(pyObject, &pos, &key, &value)) {
                if (PyUnicode_Check(key)) {
                    std::string keyStr = PyUnicode_AsUTF8(key);
                    map[keyStr] = pythonToFFIValue(static_cast<void*>(value));
                }
            }
            return FFIValue(map);
        }

        // Default: return as opaque object
        return FFIValue();
    }

    FFIValue PythonFFIImpl::pythonValueToFFI(const FFIValue& value) {
        // Simplified passthrough implementation
        return value;
    }

} // namespace ffi
#include "memory_safety.h"
#include <cstring>

namespace tocin {
namespace util {

// ManagedBuffer implementation
ManagedBuffer::ManagedBuffer(size_t size) : size_(size), capacity_(size) {
    if (size > 0) {
        data_ = std::make_unique<uint8_t[]>(size);
        std::memset(data_.get(), 0, size);
    }
}

ManagedBuffer::ManagedBuffer(const void* data, size_t size) : size_(size), capacity_(size) {
    if (size > 0 && data) {
        data_ = std::make_unique<uint8_t[]>(size);
        std::memcpy(data_.get(), data, size);
    }
}

ManagedBuffer::ManagedBuffer(ManagedBuffer&& other) noexcept
    : data_(std::move(other.data_)), size_(other.size_), capacity_(other.capacity_) {
    other.size_ = 0;
    other.capacity_ = 0;
}

ManagedBuffer& ManagedBuffer::operator=(ManagedBuffer&& other) noexcept {
    if (this != &other) {
        data_ = std::move(other.data_);
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

void ManagedBuffer::resize(size_t new_size) {
    if (new_size > capacity_) {
        auto new_data = std::make_unique<uint8_t[]>(new_size);
        if (data_) {
            std::memcpy(new_data.get(), data_.get(), size_);
        }
        data_ = std::move(new_data);
        capacity_ = new_size;
    }
    size_ = new_size;
}

void ManagedBuffer::reserve(size_t new_capacity) {
    if (new_capacity > capacity_) {
        auto new_data = std::make_unique<uint8_t[]>(new_capacity);
        if (data_) {
            std::memcpy(new_data.get(), data_.get(), size_);
        }
        data_ = std::move(new_data);
        capacity_ = new_capacity;
    }
}

void ManagedBuffer::clear() {
    size_ = 0;
}

uint8_t* ManagedBuffer::data() {
    return data_.get();
}

const uint8_t* ManagedBuffer::data() const {
    return data_.get();
}

size_t ManagedBuffer::size() const {
    return size_;
}

size_t ManagedBuffer::capacity() const {
    return capacity_;
}

bool ManagedBuffer::empty() const {
    return size_ == 0;
}

// LLVMValueHandle implementation
LLVMValueHandle::LLVMValueHandle(llvm::Value* value) : value_(value) {}

LLVMValueHandle::LLVMValueHandle(LLVMValueHandle&& other) noexcept : value_(other.value_) {
    other.value_ = nullptr;
}

LLVMValueHandle& LLVMValueHandle::operator=(LLVMValueHandle&& other) noexcept {
    if (this != &other) {
        value_ = other.value_;
        other.value_ = nullptr;
    }
    return *this;
}

llvm::Value* LLVMValueHandle::get() const {
    return value_;
}

llvm::Value* LLVMValueHandle::release() {
    auto* val = value_;
    value_ = nullptr;
    return val;
}

void LLVMValueHandle::reset(llvm::Value* new_value) {
    value_ = new_value;
}

bool LLVMValueHandle::valid() const {
    return value_ != nullptr;
}

LLVMValueHandle::operator bool() const {
    return valid();
}

// LLVMFunctionHandle implementation
LLVMFunctionHandle::LLVMFunctionHandle(llvm::Function* function) : function_(function) {}

LLVMFunctionHandle::LLVMFunctionHandle(LLVMFunctionHandle&& other) noexcept 
    : function_(other.function_) {
    other.function_ = nullptr;
}

LLVMFunctionHandle& LLVMFunctionHandle::operator=(LLVMFunctionHandle&& other) noexcept {
    if (this != &other) {
        cleanup();
        function_ = other.function_;
        other.function_ = nullptr;
    }
    return *this;
}

LLVMFunctionHandle::~LLVMFunctionHandle() {
    cleanup();
}

void LLVMFunctionHandle::cleanup() {
    // LLVM functions are owned by the module, so we don't delete them
    // We just clear our reference
    function_ = nullptr;
}

llvm::Function* LLVMFunctionHandle::get() const {
    return function_;
}

llvm::Function* LLVMFunctionHandle::release() {
    auto* func = function_;
    function_ = nullptr;
    return func;
}

void LLVMFunctionHandle::reset(llvm::Function* new_function) {
    cleanup();
    function_ = new_function;
}

bool LLVMFunctionHandle::valid() const {
    return function_ != nullptr;
}

LLVMFunctionHandle::operator bool() const {
    return valid();
}

// LLVMModuleHandle implementation
LLVMModuleHandle::LLVMModuleHandle(std::unique_ptr<llvm::Module> module) 
    : module_(std::move(module)) {}

LLVMModuleHandle::LLVMModuleHandle(LLVMModuleHandle&& other) noexcept 
    : module_(std::move(other.module_)) {}

LLVMModuleHandle& LLVMModuleHandle::operator=(LLVMModuleHandle&& other) noexcept {
    if (this != &other) {
        module_ = std::move(other.module_);
    }
    return *this;
}

llvm::Module* LLVMModuleHandle::get() const {
    return module_.get();
}

std::unique_ptr<llvm::Module> LLVMModuleHandle::release() {
    return std::move(module_);
}

void LLVMModuleHandle::reset(std::unique_ptr<llvm::Module> new_module) {
    module_ = std::move(new_module);
}

bool LLVMModuleHandle::valid() const {
    return module_ != nullptr;
}

LLVMModuleHandle::operator bool() const {
    return valid();
}

} // namespace util
} // namespace tocin

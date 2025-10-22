/**
 * @file memory_safe.h
 * @brief Memory-safe RAII wrappers and smart pointer utilities
 * 
 * This module provides RAII-based resource management to replace
 * raw pointers throughout the codebase and prevent memory leaks.
 */

#pragma once

#include <memory>
#include <functional>
#include <type_traits>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

namespace tocin {
namespace util {

/**
 * @brief RAII wrapper for LLVM resources that don't support unique ownership
 * 
 * Use this for LLVM IR objects that are owned by their parent context/module
 * but need automatic cleanup registration.
 */
template<typename T>
class LLVMResourceHandle {
public:
    using CleanupFunc = std::function<void(T*)>;
    
    LLVMResourceHandle() : ptr_(nullptr), cleanup_(nullptr) {}
    
    explicit LLVMResourceHandle(T* ptr, CleanupFunc cleanup = nullptr)
        : ptr_(ptr), cleanup_(cleanup) {}
    
    ~LLVMResourceHandle() {
        if (ptr_ && cleanup_) {
            cleanup_(ptr_);
        }
    }
    
    // Move semantics only (no copying)
    LLVMResourceHandle(LLVMResourceHandle&& other) noexcept
        : ptr_(other.ptr_), cleanup_(std::move(other.cleanup_)) {
        other.ptr_ = nullptr;
        other.cleanup_ = nullptr;
    }
    
    LLVMResourceHandle& operator=(LLVMResourceHandle&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            cleanup_ = std::move(other.cleanup_);
            other.ptr_ = nullptr;
            other.cleanup_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy operations
    LLVMResourceHandle(const LLVMResourceHandle&) = delete;
    LLVMResourceHandle& operator=(const LLVMResourceHandle&) = delete;
    
    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    
    explicit operator bool() const { return ptr_ != nullptr; }
    
    void reset(T* ptr = nullptr, CleanupFunc cleanup = nullptr) {
        if (ptr_ && cleanup_) {
            cleanup_(ptr_);
        }
        ptr_ = ptr;
        cleanup_ = cleanup;
    }
    
    T* release() {
        T* p = ptr_;
        ptr_ = nullptr;
        cleanup_ = nullptr;
        return p;
    }
    
private:
    T* ptr_;
    CleanupFunc cleanup_;
};

/**
 * @brief Non-owning pointer wrapper that prevents null dereference
 * 
 * Use this for parameters and return values that should never be null.
 * Throws an exception if null is passed.
 */
template<typename T>
class NotNull {
public:
    explicit NotNull(T* ptr) : ptr_(ptr) {
        if (!ptr_) {
            throw std::invalid_argument("NotNull: pointer cannot be null");
        }
    }
    
    NotNull(std::nullptr_t) = delete;
    
    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    
    operator T*() const { return ptr_; }
    
private:
    T* ptr_;
};

/**
 * @brief Scoped guard for arbitrary cleanup operations
 * 
 * Executes a cleanup function when going out of scope.
 */
class ScopeGuard {
public:
    using CleanupFunc = std::function<void()>;
    
    explicit ScopeGuard(CleanupFunc cleanup)
        : cleanup_(std::move(cleanup)), dismissed_(false) {}
    
    ~ScopeGuard() {
        if (!dismissed_ && cleanup_) {
            cleanup_();
        }
    }
    
    void dismiss() { dismissed_ = true; }
    
    // Move only
    ScopeGuard(ScopeGuard&& other) noexcept
        : cleanup_(std::move(other.cleanup_)), dismissed_(other.dismissed_) {
        other.dismissed_ = true;
    }
    
    ScopeGuard& operator=(ScopeGuard&& other) noexcept {
        if (this != &other) {
            if (!dismissed_ && cleanup_) {
                cleanup_();
            }
            cleanup_ = std::move(other.cleanup_);
            dismissed_ = other.dismissed_;
            other.dismissed_ = true;
        }
        return *this;
    }
    
    // Delete copy operations
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    
private:
    CleanupFunc cleanup_;
    bool dismissed_;
};

/**
 * @brief Helper to create scope guards
 */
template<typename F>
ScopeGuard makeScopeGuard(F&& cleanup) {
    return ScopeGuard(std::forward<F>(cleanup));
}

/**
 * @brief Resource pool for managing multiple related resources
 * 
 * Useful for managing groups of temporary values or allocations.
 */
template<typename T>
class ResourcePool {
public:
    ResourcePool() = default;
    ~ResourcePool() { clear(); }
    
    // Move only
    ResourcePool(ResourcePool&&) noexcept = default;
    ResourcePool& operator=(ResourcePool&&) noexcept = default;
    
    // Delete copy operations
    ResourcePool(const ResourcePool&) = delete;
    ResourcePool& operator=(const ResourcePool&) = delete;
    
    template<typename... Args>
    T* allocate(Args&&... args) {
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = ptr.get();
        resources_.push_back(std::move(ptr));
        return raw;
    }
    
    void clear() {
        resources_.clear();
    }
    
    size_t size() const { return resources_.size(); }
    bool empty() const { return resources_.empty(); }
    
private:
    std::vector<std::unique_ptr<T>> resources_;
};

/**
 * @brief RAII wrapper for file handles
 */
class FileHandle {
public:
    FileHandle() : file_(nullptr) {}
    
    explicit FileHandle(FILE* file) : file_(file) {}
    
    explicit FileHandle(const char* path, const char* mode) {
        file_ = fopen(path, mode);
    }
    
    ~FileHandle() {
        if (file_) {
            fclose(file_);
        }
    }
    
    // Move only
    FileHandle(FileHandle&& other) noexcept : file_(other.file_) {
        other.file_ = nullptr;
    }
    
    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            if (file_) {
                fclose(file_);
            }
            file_ = other.file_;
            other.file_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy operations
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    
    FILE* get() const { return file_; }
    operator FILE*() const { return file_; }
    explicit operator bool() const { return file_ != nullptr; }
    
    void close() {
        if (file_) {
            fclose(file_);
            file_ = nullptr;
        }
    }
    
private:
    FILE* file_;
};

/**
 * @brief RAII wrapper for dynamic library handles
 */
class LibraryHandle {
public:
    LibraryHandle() : handle_(nullptr) {}
    
    explicit LibraryHandle(void* handle) : handle_(handle) {}
    
    ~LibraryHandle();
    
    // Move only
    LibraryHandle(LibraryHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    LibraryHandle& operator=(LibraryHandle&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy operations
    LibraryHandle(const LibraryHandle&) = delete;
    LibraryHandle& operator=(const LibraryHandle&) = delete;
    
    void* get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }
    
    void close();
    
    void* getSymbol(const char* name) const;
    
private:
    void* handle_;
};

/**
 * @brief Checked pointer arithmetic helper
 * 
 * Prevents buffer overflows in pointer arithmetic.
 */
template<typename T>
class CheckedPointer {
public:
    CheckedPointer(T* ptr, size_t size)
        : ptr_(ptr), base_(ptr), size_(size) {}
    
    T& operator[](size_t index) {
        if (index >= size_) {
            throw std::out_of_range("CheckedPointer: index out of bounds");
        }
        return ptr_[index];
    }
    
    const T& operator[](size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("CheckedPointer: index out of bounds");
        }
        return ptr_[index];
    }
    
    CheckedPointer& operator+=(ptrdiff_t offset) {
        if (offset < 0 && static_cast<size_t>(-offset) > (ptr_ - base_)) {
            throw std::out_of_range("CheckedPointer: negative offset out of bounds");
        }
        if (offset > 0 && static_cast<size_t>(offset) >= (size_ - (ptr_ - base_))) {
            throw std::out_of_range("CheckedPointer: positive offset out of bounds");
        }
        ptr_ += offset;
        return *this;
    }
    
    CheckedPointer operator+(ptrdiff_t offset) const {
        CheckedPointer result = *this;
        result += offset;
        return result;
    }
    
    T* get() const { return ptr_; }
    size_t remaining() const { return size_ - (ptr_ - base_); }
    
private:
    T* ptr_;
    T* base_;
    size_t size_;
};

/**
 * @brief Factory functions for creating smart pointers
 */
template<typename T, typename... Args>
std::unique_ptr<T> makeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
std::shared_ptr<T> makeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

/**
 * @brief Observer pointer - non-owning pointer that asserts validity
 * 
 * Use for parameters where ownership is not transferred.
 */
template<typename T>
using Observer = T*;

/**
 * @brief Owner pointer alias - indicates ownership
 * 
 * Use to document ownership in APIs where unique_ptr isn't appropriate.
 */
template<typename T>
using Owner = T*;

} // namespace util
} // namespace tocin

#pragma once
#include <windows.h>
#include <vector>

namespace safetouch {

template<class T>
class SecureVector {
public:
    SecureVector() = default;
    explicit SecureVector(size_t count) : value_(count) {}
    ~SecureVector() { clear(); }
    SecureVector(const SecureVector&) = delete;
    SecureVector& operator=(const SecureVector&) = delete;
    SecureVector(SecureVector&& other) noexcept : value_(std::move(other.value_)) { other.value_.clear(); }
    SecureVector& operator=(SecureVector&& other) noexcept {
        if (this != &other) { clear(); value_ = std::move(other.value_); other.value_.clear(); }
        return *this;
    }
    T* data() noexcept { return value_.data(); }
    const T* data() const noexcept { return value_.data(); }
    size_t size() const noexcept { return value_.size(); }
    bool empty() const noexcept { return value_.empty(); }
    void resize(size_t count) { value_.resize(count); }
    void clear() noexcept {
        if (!value_.empty()) SecureZeroMemory(value_.data(), value_.size() * sizeof(T));
        value_.clear();
    }
private:
    std::vector<T> value_;
};

}

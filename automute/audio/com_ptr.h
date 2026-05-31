#pragma once
//
// 极简 COM 智能指针：RAII 管理 IUnknown 派生对象的引用计数。
// 学习点：构造/析构对应 AddRef/Release，移动语义转移所有权，
// operator& 用于接收 COM API 的 out 参数（**ppv）。
//
#include <utility>

template <typename T> class ComPtr {
public:
  ComPtr() = default;
  ~ComPtr() { reset(); }

  // 不可复制（避免无意的引用计数翻倍），只可移动。
  ComPtr(const ComPtr&) = delete;
  ComPtr& operator=(const ComPtr&) = delete;

  ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
  ComPtr& operator=(ComPtr&& other) noexcept {
    if (this != &other) {
      reset();
      ptr_ = other.ptr_;
      other.ptr_ = nullptr;
    }
    return *this;
  }

  T* operator->() const { return ptr_; }
  T* get() const { return ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

  // 接收 out 参数：Foo(__uuidof(T), &comptr)。先释放旧值，保证不泄漏。
  T** operator&() {
    reset();
    return &ptr_;
  }

  void reset() {
    if (ptr_) {
      ptr_->Release();
      ptr_ = nullptr;
    }
  }

private:
  T* ptr_ = nullptr;
};

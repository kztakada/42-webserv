#ifndef UTILS_OWNED_PTR_HPP_
#define UTILS_OWNED_PTR_HPP_

#include <cstddef>

namespace utils
{

// C++98 向けの最小RAIIスマートポインタ。
// - 単一所有（コピー時に所有権を移譲）
// - Result<T> が const T& を受け取って保持する都合上、
//   copy ctor / copy assign は const からも所有権を移譲する。
//   （内部ポインタを mutable にしている）
template <typename T>
class OwnedPtr
{
   public:
    explicit OwnedPtr(T* ptr = NULL) : ptr_(ptr) {}

    OwnedPtr(const OwnedPtr& rhs) : ptr_(rhs.ptr_) { rhs.ptr_ = NULL; }

    OwnedPtr& operator=(const OwnedPtr& rhs)
    {
        if (this == &rhs)
            return *this;

        reset(rhs.ptr_);
        rhs.ptr_ = NULL;
        return *this;
    }

    ~OwnedPtr() { delete ptr_; }

    T* get() const { return ptr_; }

    T* release() const
    {
        T* tmp = ptr_;
        ptr_ = NULL;
        return tmp;
    }

    void reset(T* ptr = NULL)
    {
        if (ptr_ == ptr)
            return;
        delete ptr_;
        ptr_ = ptr;
    }

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }

   private:
    mutable T* ptr_;
};

}  // namespace utils

#endif

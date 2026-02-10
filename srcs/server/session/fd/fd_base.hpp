#ifndef WEBSERV_FD_BASE_HPP_
#define WEBSERV_FD_BASE_HPP_

#include <unistd.h>

namespace server
{

class FdBase
{
   protected:
    int fd_;

   public:
    explicit FdBase(int fd) : fd_(fd) {}
    virtual ~FdBase()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }

    int getFd() const { return fd_; }

    // FD の所有権を呼び出し側へ移譲する。
    // 以降、このオブジェクトのデストラクタでは close されない。
    int release()
    {
        const int released = fd_;
        fd_ = -1;
        return released;
    }

   private:
    FdBase();                          // デフォルトコンストラクタ禁止
    FdBase(const FdBase&);             // コピー禁止
    FdBase& operator=(const FdBase&);  // 代入禁止
};

}  // namespace server

#endif

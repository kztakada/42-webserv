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

   private:
    FdBase();                          // デフォルトコンストラクタ禁止
    FdBase(const FdBase&);             // コピー禁止
    FdBase& operator=(const FdBase&);  // 代入禁止
};

}  // namespace server

#endif
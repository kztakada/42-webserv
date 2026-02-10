#ifndef WEBSERV_IO_BUFFER_HPP_
#define WEBSERV_IO_BUFFER_HPP_

#include <sys/types.h>

#include <cstddef>
#include <string>
#include <vector>

#include "utils/byte.hpp"

namespace server
{

class IoBuffer
{
   private:
    std::vector<utils::Byte> storage_;
    size_t read_pos_;   // 次に読み出す位置
    size_t write_pos_;  // 次に書き込む（追記する）位置

   public:
    IoBuffer();
    ~IoBuffer();

    // ソケット/パイプから読み込んでバッファに追記
    ssize_t fillFromFd(int fd);

    // バッファの内容をソケット/パイプに書き出す
    ssize_t flushToFd(int fd);

    // データの取得、消費（ポインタ操作）などのユーティリティ
    const char* data() const;
    size_t size() const { return write_pos_ - read_pos_; }
    void consume(size_t n);  // 処理済みデータを捨てる

    // テストや組み立て用途でバッファへ追記
    void append(const char* data, size_t n);
    void append(const std::string& s);
};

}  // namespace server

#endif

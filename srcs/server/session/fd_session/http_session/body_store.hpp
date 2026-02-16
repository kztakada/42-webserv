#ifndef WEBSERV_BODY_STORE_HPP_
#define WEBSERV_BODY_STORE_HPP_

#include <cstddef>
#include <string>

#include "utils/data_type.hpp"
#include "utils/result.hpp"

namespace server
{
using utils::result::Result;
class BodyStore
{
   public:
    explicit BodyStore(const void* unique_key);
    ~BodyStore();

    // upload_store 向けに出力先を差し替える。
    // - reset() では削除しない（アップロード結果を保持する）
    // - allow_overwrite=false の場合、既存ファイルがあると open に失敗する
    Result<void> configureForUpload(
        const std::string& destination_path, bool allow_overwrite);

    Result<void> begin();
    Result<void> append(const utils::Byte* data, size_t len);
    Result<void> finish();

    Result<int> openForRead() const;

    // unlink/remove の代替として std::remove を許容する前提。
    // 失敗した場合は ERROR を返す（未作成/既に削除済みは OK 扱い）。
    Result<void> removeFile();

    const std::string& path() const { return path_; }
    size_t size() const { return size_bytes_; }

    void reset();
    void commit() { is_committed_ = true; }

   private:
    std::string default_path_;
    std::string path_;
    int write_fd_;
    size_t size_bytes_;

    bool remove_on_reset_;
    bool allow_overwrite_;
    bool is_committed_;

    BodyStore();
    BodyStore(const BodyStore& rhs);
    BodyStore& operator=(const BodyStore& rhs);

    static std::string buildPath_(const void* unique_key);
};

}  // namespace server

#endif

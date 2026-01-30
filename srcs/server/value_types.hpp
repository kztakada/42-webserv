#ifndef WEBSERV_SERVER_VALUE_TYPES_HPP_
#define WEBSERV_SERVER_VALUE_TYPES_HPP_

#include <string>

#include "utils/path.hpp"

namespace server
{

typedef std::string FileName;

// URI: "/"から始まるパス
// URL: "http://"または"https://"から始まるパス
typedef std::string TargetURI;  // URIまたはURL
typedef std::string URIPath;    // URIのみ
typedef std::string CgiExt;

// conf_spec.md 6章「物理パス系」: 正規化済み絶対パス
typedef utils::path::PhysicalPath FilePath;

}  // namespace server

#endif

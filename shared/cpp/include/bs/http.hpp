#pragma once
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bs {

struct Config {
  std::string root;     // best-stack directory (static/, templates/)
  std::string db_path;  // data/app.sqlite
  int threads = 8;
};

struct Req {
  std::string method, path;
  std::map<std::string, std::string> params;   // from route pattern {name}
  std::map<std::string, std::string> query;    // decoded
  std::map<std::string, std::string> form;     // decoded application/x-www-form-urlencoded
  std::map<std::string, std::string> headers;  // lower-case keys
  std::map<std::string, std::string> cookies;
  std::string body;                            // raw body (JSON for /api)
};

struct Resp {
  int status = 200;
  std::string content_type = "text/html; charset=utf-8";
  std::string body;
  std::vector<std::pair<std::string, std::string>> headers;
};

using Handler = Resp (*)(const Req&);
struct Route {
  const char* method;
  const char* pattern;  // "/{fe}/catalog/p/{id}"; "{x}" matches one non-empty segment
  Handler fn;
};

/// Single source of truth for all routes. Backends register these natively.
const std::vector<Route>& routes();

/// Must be called once before serving: sets DB path, loads templates.
void init(const Config& cfg);

/// GET /static/<rel>; nullopt when not found or path escapes the static dir.
std::optional<Resp> serve_static(const std::string& rel_path);

/// Helpers backends may reuse for parsing.
bool match(const std::string& pattern, const std::string& path, std::map<std::string, std::string>& params);
std::map<std::string, std::string> parse_urlencoded(const std::string& s);
std::map<std::string, std::string> parse_cookies(const std::string& header);
std::string url_decode(const std::string& s);

}  // namespace bs

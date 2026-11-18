// Drogon backend: event-loop threads (app().setThreadNum); routes registered natively with
// Drogon's {placeholder} path syntax from bs::routes().
#include <drogon/drogon.h>

#include <cstdio>
#include <string>

#include "bs/http.hpp"

using namespace drogon;

namespace {
HttpMethod to_method(const std::string& m) {
  if (m == "POST") return Post;
  if (m == "PATCH") return Patch;
  if (m == "DELETE") return Delete;
  return Get;
}

std::vector<std::string> param_names(const std::string& pattern) {
  std::vector<std::string> out;
  for (size_t i = pattern.find('{'); i != std::string::npos; i = pattern.find('{', i + 1))
    out.push_back(pattern.substr(i + 1, pattern.find('}', i) - i - 1));
  return out;
}

bs::Req to_req(const HttpRequestPtr& r) {
  bs::Req q;
  q.method = r->methodString();
  q.path = r->path();
  q.query = bs::parse_urlencoded(std::string(r->query()));
  for (auto& [k, v] : r->headers()) q.headers[k] = v;  // drogon lower-cases header names
  for (auto& [k, v] : r->cookies()) q.cookies[k] = v;
  q.body = std::string(r->body());
  if (r->getHeader("content-type").rfind("application/x-www-form-urlencoded", 0) == 0) q.form = bs::parse_urlencoded(q.body);
  return q;
}

HttpResponsePtr to_resp(const bs::Resp& in) {
  auto res = HttpResponse::newHttpResponse();
  res->setStatusCode(static_cast<HttpStatusCode>(in.status));
  res->setContentTypeString(in.content_type);
  for (auto& [k, v] : in.headers) res->addHeader(k, v);
  res->setBody(in.body);
  return res;
}

using Callback = std::function<void(const HttpResponsePtr&)>;
}  // namespace

int main(int argc, char** argv) {
  auto cli = bs::parse_cli(argc, argv);
  bs::init(cli.cfg);

  for (const auto& rt : bs::routes()) {
    auto fn = rt.fn;
    auto names = param_names(rt.pattern);
    auto method = to_method(rt.method);
    switch (names.size()) {
      case 0:
        app().registerHandler(rt.pattern, [fn](const HttpRequestPtr& req, Callback&& cb) { cb(to_resp(fn(to_req(req)))); }, {method});
        break;
      case 1:
        app().registerHandler(rt.pattern,
            [fn, names](const HttpRequestPtr& req, Callback&& cb, const std::string& a) {
              auto q = to_req(req);
              q.params[names[0]] = a;
              cb(to_resp(fn(q)));
            }, {method});
        break;
      case 2:
        app().registerHandler(rt.pattern,
            [fn, names](const HttpRequestPtr& req, Callback&& cb, const std::string& a, const std::string& b) {
              auto q = to_req(req);
              q.params[names[0]] = a;
              q.params[names[1]] = b;
              cb(to_resp(fn(q)));
            }, {method});
        break;
    }
  }
  app().registerHandlerViaRegex("/static/(.+)",
      [](const HttpRequestPtr&, Callback&& cb, const std::string& rel) {
        auto r = bs::serve_static(bs::url_decode(rel));
        cb(to_resp(r ? *r : bs::Resp{404, "text/plain", "not found", {}}));
      }, {Get});

  std::printf("bs-drogon listening on :%d (threads=%d)\n", cli.port, cli.cfg.threads);
  std::fflush(stdout);
  app().setLogLevel(trantor::Logger::kWarn)
      .setThreadNum(cli.cfg.threads)
      .disableSession()
      .setServerHeaderField("")
      .addListener("0.0.0.0", cli.port)
      .run();
  return 0;
}

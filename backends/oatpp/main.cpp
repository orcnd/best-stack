// Oat++ backend: HttpRouter with {name} path variables (same syntax as bs::routes()) and the
// synchronous HttpConnectionHandler (one thread per connection; see NOTES.md).
#include <oatpp/core/base/Environment.hpp>
#include <oatpp/network/Server.hpp>
#include <oatpp/network/tcp/server/ConnectionProvider.hpp>
#include <oatpp/web/server/HttpConnectionHandler.hpp>
#include <oatpp/web/server/HttpRouter.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "bs/http.hpp"

using oatpp::web::protocol::http::Status;
using oatpp::web::protocol::http::incoming::Request;
using oatpp::web::protocol::http::outgoing::Response;
using oatpp::web::protocol::http::outgoing::ResponseFactory;

namespace {
const char* reason(int code) {
  switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 303: return "See Other";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 422: return "Unprocessable Entity";
    default: return "Error";
  }
}

std::vector<std::string> param_names(const std::string& pattern) {
  std::vector<std::string> out;
  for (size_t i = pattern.find('{'); i != std::string::npos; i = pattern.find('{', i + 1))
    out.push_back(pattern.substr(i + 1, pattern.find('}', i) - i - 1));
  return out;
}

bs::Req to_req(const std::shared_ptr<Request>& r, const std::vector<std::string>& names) {
  bs::Req q;
  q.method = r->getStartingLine().method.toString();
  std::string target = r->getStartingLine().path.toString();
  auto qm = target.find('?');
  q.path = target.substr(0, qm);
  if (qm != std::string::npos) q.query = bs::parse_urlencoded(target.substr(qm + 1));
  for (auto& n : names) {
    auto v = r->getPathVariable(n.c_str());
    if (v) q.params[n] = *v;
  }
  for (auto& [k, v] : r->getHeaders().getAll()) {
    std::string lk = k.std_str();
    std::transform(lk.begin(), lk.end(), lk.begin(), [](unsigned char c) { return std::tolower(c); });
    q.headers[lk] = v.std_str();
  }
  q.cookies = bs::parse_cookies(bs::detail_get_header(q.headers, "cookie"));
  // Oat++'s body decoder rejects body-less requests (e.g. fetch DELETE) without a length header.
  if (q.headers.count("content-length") || q.headers.count("transfer-encoding")) {
    oatpp::String body = r->readBodyToString();
    if (body) q.body = *body;
  }
  if (bs::detail_get_header(q.headers, "content-type").rfind("application/x-www-form-urlencoded", 0) == 0)
    q.form = bs::parse_urlencoded(q.body);
  return q;
}

std::shared_ptr<Response> to_resp(const bs::Resp& in) {
  auto res = ResponseFactory::createResponse(Status(in.status, reason(in.status)), oatpp::String(in.body));
  res->putHeader("Content-Type", in.content_type.c_str());
  for (auto& [k, v] : in.headers) res->putHeader(k.c_str(), v.c_str());
  return res;
}

class RouteHandler : public oatpp::web::server::HttpRequestHandler {
 public:
  RouteHandler(bs::Handler fn, std::vector<std::string> names) : fn_(fn), names_(std::move(names)) {}
  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override {
    try {
      return to_resp(fn_(to_req(request, names_)));
    } catch (const std::exception& e) {
      std::fprintf(stderr, "handler error: %s\n", e.what());
      return to_resp(bs::Resp{500, "text/plain", "internal error", {}});
    }
  }

 private:
  bs::Handler fn_;
  std::vector<std::string> names_;
};

class StaticHandler : public oatpp::web::server::HttpRequestHandler {
 public:
  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override {
    auto tail = request->getPathTail();
    std::string rel = tail ? bs::url_decode(*tail) : "";
    auto q = rel.find('?');
    if (q != std::string::npos) rel = rel.substr(0, q);
    auto r = bs::serve_static(rel);
    return to_resp(r ? *r : bs::Resp{404, "text/plain", "not found", {}});
  }
};
}  // namespace

int main(int argc, char** argv) {
  auto cli = bs::parse_cli(argc, argv);
  bs::init(cli.cfg);
  oatpp::base::Environment::init();
  {
    auto router = oatpp::web::server::HttpRouter::createShared();
    for (const auto& rt : bs::routes())
      router->route(rt.method, rt.pattern, std::make_shared<RouteHandler>(rt.fn, param_names(rt.pattern)));
    router->route("GET", "/static/*", std::make_shared<StaticHandler>());

    auto handler = oatpp::web::server::HttpConnectionHandler::createShared(router);
    auto provider = oatpp::network::tcp::server::ConnectionProvider::createShared(
        {"0.0.0.0", (v_uint16)cli.port, oatpp::network::Address::IP_4});
    oatpp::network::Server server(provider, handler);
    std::printf("bs-oatpp listening on :%d (thread per connection)\n", cli.port);
    std::fflush(stdout);
    server.run();
  }
  oatpp::base::Environment::destroy();
  return 0;
}

// Crow backend: multithreaded app (concurrency = --threads); routes registered natively as
// dynamic rules with Crow's <string> segments, translated from bs::routes().
#include <crow.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "bs/http.hpp"

namespace {
std::vector<std::string> param_names(const std::string& pattern) {
  std::vector<std::string> out;
  for (size_t i = pattern.find('{'); i != std::string::npos; i = pattern.find('{', i + 1))
    out.push_back(pattern.substr(i + 1, pattern.find('}', i) - i - 1));
  return out;
}

std::string to_crow_pattern(const std::string& p) {
  std::string out;
  for (size_t i = 0; i < p.size(); i++) {
    if (p[i] == '{') {
      out += "<string>";
      i = p.find('}', i);
    } else {
      out += p[i];
    }
  }
  return out;
}

crow::HTTPMethod to_method(const std::string& m) {
  if (m == "POST") return crow::HTTPMethod::Post;
  if (m == "PATCH") return crow::HTTPMethod::Patch;
  if (m == "DELETE") return crow::HTTPMethod::Delete;
  return crow::HTTPMethod::Get;
}

bs::Req to_req(const crow::request& r) {
  bs::Req q;
  q.method = crow::method_name(r.method);
  q.path = r.url;
  auto qm = r.raw_url.find('?');
  if (qm != std::string::npos) q.query = bs::parse_urlencoded(r.raw_url.substr(qm + 1));
  for (auto& [k, v] : r.headers) {
    std::string lk = k;
    std::transform(lk.begin(), lk.end(), lk.begin(), [](unsigned char c) { return std::tolower(c); });
    q.headers[lk] = v;
  }
  q.cookies = bs::parse_cookies(r.get_header_value("Cookie"));
  q.body = r.body;
  if (r.get_header_value("Content-Type").rfind("application/x-www-form-urlencoded", 0) == 0)
    q.form = bs::parse_urlencoded(r.body);
  return q;
}

crow::response to_resp(const bs::Resp& in) {
  crow::response res(in.status);
  res.set_header("Content-Type", in.content_type);
  for (auto& [k, v] : in.headers) res.add_header(k, v);
  res.body = in.body;
  return res;
}
}  // namespace

int main(int argc, char** argv) {
  auto cli = bs::parse_cli(argc, argv);
  bs::init(cli.cfg);

  crow::SimpleApp app;
  app.loglevel(crow::LogLevel::Warning);

  for (const auto& rt : bs::routes()) {
    auto fn = rt.fn;
    auto names = param_names(rt.pattern);
    auto& rule = app.route_dynamic(to_crow_pattern(rt.pattern)).methods(to_method(rt.method));
    switch (names.size()) {
      case 0:
        rule([fn](const crow::request& req) { return to_resp(fn(to_req(req))); });
        break;
      case 1:
        rule([fn, names](const crow::request& req, std::string a) {
          auto q = to_req(req);
          q.params[names[0]] = a;
          return to_resp(fn(q));
        });
        break;
      case 2:
        rule([fn, names](const crow::request& req, std::string a, std::string b) {
          auto q = to_req(req);
          q.params[names[0]] = a;
          q.params[names[1]] = b;
          return to_resp(fn(q));
        });
        break;
    }
  }
  app.route_dynamic("/static/<path>")([](const crow::request&, std::string rel) {
    auto r = bs::serve_static(bs::url_decode(rel));
    return to_resp(r ? *r : bs::Resp{404, "text/plain", "not found", {}});
  });

  std::printf("bs-crow listening on :%d (threads=%d)\n", cli.port, cli.cfg.threads);
  std::fflush(stdout);
  app.bindaddr("0.0.0.0").port(cli.port).concurrency(cli.cfg.threads).run();
  return 0;
}

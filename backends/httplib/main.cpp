// cpp-httplib backend: thread-pool server; routes registered natively from bs::routes().
#include <httplib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "bs/http.hpp"

namespace {
std::string to_httplib_pattern(std::string p) {
  std::string out;
  for (size_t i = 0; i < p.size(); i++) {
    if (p[i] == '{') {
      auto close = p.find('}', i);
      out += ':' + p.substr(i + 1, close - i - 1);
      i = close;
    } else {
      out += p[i];
    }
  }
  return out;
}

bs::Req to_req(const httplib::Request& r) {
  bs::Req q;
  q.method = r.method;
  q.path = r.path;
  for (auto& [k, v] : r.path_params) q.params[k] = v;
  auto qm = r.target.find('?');
  if (qm != std::string::npos) q.query = bs::parse_urlencoded(r.target.substr(qm + 1));
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

void write(const bs::Resp& in, httplib::Response& out) {
  out.status = in.status;
  for (auto& [k, v] : in.headers) out.set_header(k, v);
  out.set_content(in.body, in.content_type);
}
}  // namespace

int main(int argc, char** argv) {
  auto cli = bs::parse_cli(argc, argv);
  bs::init(cli.cfg);

  httplib::Server svr;
  // httplib is thread-per-connection: a keep-alive client holds a pool thread, so the pool is
  // sized for concurrent connections (64), not cores (--threads). See backends/httplib/NOTES.md.
  const int threads = std::max(cli.cfg.threads, 64);
  svr.new_task_queue = [threads] { return new httplib::ThreadPool(threads); };

  for (const auto& rt : bs::routes()) {
    auto fn = rt.fn;
    auto handler = [fn](const httplib::Request& req, httplib::Response& res) { write(fn(to_req(req)), res); };
    std::string m = rt.method, pat = to_httplib_pattern(rt.pattern);
    if (m == "GET") svr.Get(pat, handler);
    else if (m == "POST") svr.Post(pat, handler);
    else if (m == "PATCH") svr.Patch(pat, handler);
    else if (m == "DELETE") svr.Delete(pat, handler);
  }
  svr.Get(R"(/static/(.+))", [](const httplib::Request& req, httplib::Response& res) {
    auto r = bs::serve_static(bs::url_decode(req.matches[1].str()));
    if (r) write(*r, res);
    else { res.status = 404; res.set_content("not found", "text/plain"); }
  });

  std::printf("bs-httplib listening on :%d (threads=%d)\n", cli.port, threads);
  std::fflush(stdout);
  return svr.listen("0.0.0.0", cli.port) ? 0 : 1;
}

// uWebSockets backend. Model chosen by the macOS spike (see NOTES.md).
#include <App.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "bs/http.hpp"

namespace {
const char* status_line(int code) {
  switch (code) {
    case 200: return "200 OK";
    case 201: return "201 Created";
    case 204: return "204 No Content";
    case 303: return "303 See Other";
    case 400: return "400 Bad Request";
    case 404: return "404 Not Found";
    case 422: return "422 Unprocessable Entity";
    default: return "500 Internal Server Error";
  }
}

std::string to_uws_pattern(const std::string& p) {
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

std::vector<std::string> param_names(const std::string& pattern) {
  std::vector<std::string> out;
  for (size_t i = pattern.find('{'); i != std::string::npos; i = pattern.find('{', i + 1))
    out.push_back(pattern.substr(i + 1, pattern.find('}', i) - i - 1));
  return out;
}

/// Copies everything needed out of uWS::HttpRequest (valid only during the callback).
bs::Req read_req(uWS::HttpRequest* r, const std::vector<std::string>& names) {
  bs::Req q;
  q.method = std::string(r->getCaseSensitiveMethod());
  q.path = std::string(r->getUrl());
  q.query = bs::parse_urlencoded(std::string(r->getQuery()));
  for (size_t i = 0; i < names.size(); i++) q.params[names[i]] = std::string(r->getParameter((unsigned)i));
  for (auto [k, v] : *r) q.headers[std::string(k)] = std::string(v);  // uWS lower-cases header names
  q.cookies = bs::parse_cookies(bs::detail_get_header(q.headers, "cookie"));
  return q;
}

/// Never let an exception unwind into uSockets' C event loop (that terminates the process).
bs::Resp safe_call(bs::Handler fn, const bs::Req& q) {
  try {
    return fn(q);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "handler error: %s\n", e.what());
    return bs::Resp{500, "text/plain", "internal error", {}};
  }
}

template <class Res>
void write(Res* res, const bs::Resp& out) {
  res->cork([&] {
    res->writeStatus(status_line(out.status));
    res->writeHeader("Content-Type", out.content_type);
    for (auto& [k, v] : out.headers) res->writeHeader(k, v);
    res->end(out.body);
  });
}

std::atomic<long>* g_counts = nullptr;

void add_routes(uWS::App& app, int thread_index) {
  for (const auto& rt : bs::routes()) {
    auto fn = rt.fn;
    auto names = param_names(rt.pattern);
    std::string method = rt.method;
    auto pattern = to_uws_pattern(rt.pattern);
    auto handler = [fn, names, method, thread_index](auto* res, auto* req) {
      if (g_counts) g_counts[thread_index]++;
      auto q = std::make_shared<bs::Req>(read_req(req, names));
      if (method == "GET") {
        write(res, safe_call(fn, *q));
        return;
      }
      // Collect the body, then run the handler.
      auto aborted = std::make_shared<bool>(false);
      res->onAborted([aborted] { *aborted = true; });
      res->onData([res, q, fn, aborted](std::string_view chunk, bool last) {
        q->body.append(chunk);
        if (!last || *aborted) return;
        if (bs::detail_get_header(q->headers, "content-type").rfind("application/x-www-form-urlencoded", 0) == 0)
          q->form = bs::parse_urlencoded(q->body);
        write(res, safe_call(fn, *q));
      });
    };
    if (method == "GET") app.get(pattern, handler);
    else if (method == "POST") app.post(pattern, handler);
    else if (method == "PATCH") app.patch(pattern, handler);
    else if (method == "DELETE") app.del(pattern, handler);
  }
  app.get("/static/*", [](auto* res, auto* req) {
    auto url = std::string(req->getUrl());
    auto r = bs::serve_static(bs::url_decode(url.substr(8)));
    write(res, r ? *r : bs::Resp{404, "text/plain", "not found", {}});
  });
  app.any("/*", [](auto* res, auto*) { write(res, bs::Resp{404, "text/plain", "not found", {}}); });
}
}  // namespace

int main(int argc, char** argv) {
  auto cli = bs::parse_cli(argc, argv);
  bs::init(cli.cfg);
  const int n = cli.cfg.threads;
  const bool stats = std::getenv("BS_UWS_STATS") != nullptr;
  if (stats) g_counts = new std::atomic<long>[n]();

  // macOS SO_REUSEPORT does not balance (spike: all requests landed on one of 8 loops), so use
  // uWS's own load balancer: an acceptor App distributes accepted sockets round-robin to N
  // worker Apps (addChildApp), each running its own event loop thread. See NOTES.md.
  std::vector<uWS::App*> workers(n, nullptr);
  std::mutex mu;
  std::condition_variable cv;
  int ready = 0;
  std::vector<std::thread> threads;
  for (int i = 0; i < n; i++) {
    threads.emplace_back([i, &workers, &mu, &cv, &ready] {
      auto* app = new uWS::App();
      add_routes(*app, i);
      // A worker loop has no listen socket of its own; without a pending timer uWS::run()
      // would return immediately. Keep it alive with a long no-op timer.
      auto* keepalive = us_create_timer((us_loop_t*)uWS::Loop::get(), 0, 0);
      us_timer_set(keepalive, [](us_timer_t*) {}, 3600 * 1000, 3600 * 1000);
      {
        std::lock_guard lk(mu);
        workers[i] = app;
        ready++;
      }
      cv.notify_one();
      app->run();
    });
  }
  {
    std::unique_lock lk(mu);
    cv.wait(lk, [&] { return ready == n; });
  }
  uWS::App acceptor;
  for (auto* w : workers) acceptor.addChildApp(w);
  acceptor.listen(cli.port, [&cli](auto* sock) {
    if (!sock) {
      std::fprintf(stderr, "listen failed on %d\n", cli.port);
      std::exit(1);
    }
  });
  std::printf("bs-uwebsockets listening on :%d (workers=%d, acceptor + addChildApp)\n", cli.port, n);
  std::fflush(stdout);
  if (stats) {
    std::thread([n] {
    for (;;) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      std::string line = "per-thread requests:";
      for (int i = 0; i < n; i++) line += " " + std::to_string(g_counts[i].load());
      std::fprintf(stderr, "%s\n", line.c_str());
    }
    }).detach();
  }
  acceptor.run();
  for (auto& t : threads) t.join();
  return 0;
}

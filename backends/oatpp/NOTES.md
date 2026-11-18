# Oat++ notes

- Model: synchronous `HttpConnectionHandler` — one thread per open connection (no fixed pool), so
  `--threads` does not apply. Oat++'s async API (coroutines) would need non-blocking DB access;
  with the shared blocking SQLite layer the sync handler is the natural fit.
- Body is read only when `Content-Length`/`Transfer-Encoding` is present: Oat++'s body decoder
  rejects body-less requests (e.g. `fetch(..., {method: "DELETE"})`) otherwise.
- oatpp 1.3.1 declares `cmake_minimum_required` < 3.5; built with `CMAKE_POLICY_VERSION_MINIMUM=3.5`.

# cpp-httplib notes

- Model: blocking, thread-per-connection via `ThreadPool`. An idle keep-alive connection occupies a
  pool thread until its keep-alive timeout (5 s). With a pool of 8, the 9th concurrent keep-alive
  client waits up to 5 s, so the pool is sized to 64 (the highest load-test concurrency).
- Routes come from `bs::routes()`, converted from `{name}` to httplib's `:name` path params.

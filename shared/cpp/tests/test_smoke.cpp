#include <doctest/doctest.h>
#include <inja/inja.hpp>
#include <sqlite3.h>

TEST_CASE("smoke: sqlite is recent enough") {
  CHECK(sqlite3_libversion_number() >= 3045000);
}

TEST_CASE("smoke: inja renders") {
  nlohmann::json data{{"x", 1}};
  CHECK(inja::render("{{ x }}", data) == "1");
}

TEST_CASE("smoke: sqlite build suits one-connection-per-thread servers") {
  // ENABLE_MEMORY_MANAGEMENT makes all connections share one page-cache mutex (serialized readers).
  CHECK(sqlite3_compileoption_used("ENABLE_MEMORY_MANAGEMENT") == 0);
  CHECK(sqlite3_threadsafe() == 2);  // multi-thread: connections are never shared between threads
}

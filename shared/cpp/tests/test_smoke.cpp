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

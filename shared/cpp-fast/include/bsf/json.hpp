#pragma once
// JSON for the fast core: glaze serializes the model structs directly into the response body.
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "bs/model.hpp"

namespace bsf::json {

struct ProductsResp {
  std::vector<bs::Product> items;
  int total = 0, page = 1, pages = 1;
  std::vector<bs::Category> categories;
};

/// Each overwrites `out` with the serialized value.
void write(std::string& out, const bs::Product& v);
void write(std::string& out, const ProductsResp& v);
void write(std::string& out, const bs::Cart& v);
void write(std::string& out, const bs::Card& v);
void write(std::string& out, const std::vector<bs::BoardSummary>& v);
void write(std::string& out, const bs::Board& v);  // columns must already hold all kColumns() keys
void write(std::string& out, const bs::Dashboard& v);

/// A parsed request body (top-level JSON object) with the nlohmann-style accessors the handlers need.
class Body {
 public:
  /// nullopt unless the text is one valid JSON object (trailing whitespace only).
  static std::optional<Body> parse(std::string_view text);
  ~Body();
  Body(Body&&) noexcept;
  Body& operator=(Body&&) noexcept;

  bool contains(const char* key) const;
  /// JSON integer (no fraction/exponent): its value, wrapped to int64 like nlohmann's get<int64_t>().
  std::optional<int64_t> integer(const char* key) const;
  std::optional<std::string> string(const char* key) const;

 private:
  struct Impl;
  explicit Body(Impl* p);
  Impl* p_;
};

}  // namespace bsf::json

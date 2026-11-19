#include "bsf/json.hpp"

#include <glaze/glaze.hpp>

namespace bsf::json {
// Not in an anonymous namespace: glaze's reflection needs types with linkage.
struct Kpis {
  long events = 0, users = 0, revenue_cents = 0;
  double conversion = 0;
};
struct Country {
  std::string_view country;
  long events = 0;
};
}  // namespace bsf::json

// Product, Category, Cart, CartItem, Card, BoardSummary, Board, DayPoint and Dashboard::Item are
// aggregates whose member names are the JSON keys, so glaze reflects them without metadata.
template <>
struct glz::meta<bs::Dashboard> {
  using T = bs::Dashboard;
  static constexpr auto value = glz::object(
      "range_days", &T::range_days,
      "kpis", [](auto& s) { return bsf::json::Kpis{s.events, s.users, s.revenue_cents, s.conversion}; },
      "daily", &T::daily,
      "countries",
      [](auto& s) {
        std::vector<bsf::json::Country> v;
        v.reserve(s.countries.size());
        for (auto& [c, n] : s.countries) v.push_back({c, n});
        return v;
      },
      "top_items", &T::top_items);
};

namespace bsf::json {
namespace {
template <class T>
void write_any(std::string& out, const T& v) {
  if (glz::write_json(v, out)) out = R"({"error":"serialize"})";
}
}  // namespace

void write(std::string& out, const bs::Product& v) { write_any(out, v); }
void write(std::string& out, const ProductsResp& v) { write_any(out, v); }
void write(std::string& out, const bs::Cart& v) { write_any(out, v); }
void write(std::string& out, const bs::Card& v) { write_any(out, v); }
void write(std::string& out, const std::vector<bs::BoardSummary>& v) { write_any(out, v); }
void write(std::string& out, const bs::Board& v) { write_any(out, v); }
void write(std::string& out, const bs::Dashboard& v) { write_any(out, v); }

// generic_u64 keeps integers as uint64_t / int64_t and everything else with a fraction or exponent
// as double: the same integer/float split as nlohmann's is_number_integer().
// Like nlohmann::json::parse: anything but whitespace after the value is an error.
struct BodyOpts : glz::opts {
  bool validate_trailing_whitespace = true;
};

struct Body::Impl {
  glz::generic_u64 v;
};

Body::Body(Impl* p) : p_(p) {}
Body::~Body() { delete p_; }
Body::Body(Body&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
Body& Body::operator=(Body&& o) noexcept {
  std::swap(p_, o.p_);
  return *this;
}

std::optional<Body> Body::parse(std::string_view text) {
  auto* impl = new Impl;
  if (glz::read<BodyOpts{}>(impl->v, text) || !impl->v.is_object()) {
    delete impl;
    return std::nullopt;
  }
  return Body(impl);
}

bool Body::contains(const char* key) const { return p_->v.contains(key); }

std::optional<int64_t> Body::integer(const char* key) const {
  if (!contains(key)) return std::nullopt;
  auto& x = p_->v[key];
  if (x.is_int64()) return x.get<int64_t>();
  if (x.is_uint64()) return static_cast<int64_t>(x.get<uint64_t>());
  return std::nullopt;
}

std::optional<std::string> Body::string(const char* key) const {
  if (!contains(key)) return std::nullopt;
  auto& x = p_->v[key];
  if (!x.is_string()) return std::nullopt;
  return x.get<std::string>();
}

}  // namespace bsf::json

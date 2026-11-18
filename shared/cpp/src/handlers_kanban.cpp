#include "view.hpp"

namespace bs::h {
using namespace bs::detail;
using namespace bs::view;

namespace {
Resp board_fragment(const std::string& fe, int board_id) {
  auto& db = thread_db();
  auto b = db.board(board_id);
  if (!b) return not_found_html();
  auto vm = board_vm(*b, db.boards());
  vm["fragment"] = true;
  return html(view::view(fe, "board", vm));
}

Resp card_json(const Card& c, int status = 200) { return json_resp(json(c).dump(), status); }
Resp unprocessable() { return json_resp(R"({"error":"invalid_title"})", 422); }
}  // namespace

Resp kanban_page(const Req& r) {
  auto fe = get(r.params, "fe");
  auto id = to_int(get(r.params, "board_id"));
  if (!valid_fe(fe) || !id) return not_found_html();
  auto& db = thread_db();
  auto b = db.board(*id);
  if (!b) return not_found_html();
  auto vm = view::page(r, "kanban", b->name + " · Kanban", "Kanban panosu");
  vm.update(board_vm(*b, db.boards()));
  return html(view::view(fe, "kanban", vm));
}

Resp card_create_form(const Req& r) {
  auto fe = get(r.params, "fe");
  auto id = to_int(get(r.params, "board_id"));
  if (!valid_fe(fe) || !id) return not_found_html();
  auto title = valid_title(get(r.form, "title"));
  if (!title) return html("Geçersiz başlık", 422);
  auto col = get(r.form, "column");
  if (col.empty()) col = "backlog";
  auto& db = thread_db();
  if (!db.board(*id)) return not_found_html();
  if (!db.card_create(*id, *title, col)) return html("Geçersiz sütun", 422);
  if (is_htmx(r)) return board_fragment(fe, *id);
  return redirect("/" + fe + "/kanban/" + std::to_string(*id));
}

Resp card_update_form(const Req& r) {
  auto fe = get(r.params, "fe");
  auto id = to_int(get(r.params, "id"));
  if (!valid_fe(fe) || !id) return not_found_html();
  auto& db = thread_db();
  auto card = db.card_update(*id, std::nullopt, std::nullopt);
  if (!card) return not_found_html();
  if (get(r.form, "delete") == "1") {
    db.card_delete(*id);
  } else {
    std::optional<std::string> title, move;
    if (r.form.count("title")) {
      title = valid_title(get(r.form, "title"));
      if (!title) return html("Geçersiz başlık", 422);
    }
    auto mv = get(r.form, "move");
    if (mv == "left" || mv == "right") move = mv;
    db.card_update(*id, title, move);
  }
  if (is_htmx(r)) return board_fragment(fe, card->board_id);
  return redirect("/" + fe + "/kanban/" + std::to_string(card->board_id));
}

Resp api_boards(const Req&) { return json_resp(json(thread_db().boards()).dump()); }

Resp api_board(const Req& r) {
  auto id = to_int(get(r.params, "id"));
  if (!id) return not_found_json();
  auto b = thread_db().board(*id);
  return b ? json_resp(json(*b).dump()) : not_found_json();
}

Resp api_card_create(const Req& r) {
  auto id = to_int(get(r.params, "id"));
  if (!id) return not_found_json();
  auto body = json::parse(r.body, nullptr, false);
  if (body.is_discarded() || !body.is_object()) return json_resp(R"({"error":"bad_request"})", 400);
  if (!body.contains("title") || !body["title"].is_string()) return unprocessable();
  auto title = valid_title(body["title"].get<std::string>());
  if (!title) return unprocessable();
  std::string col = body.contains("column") && body["column"].is_string() ? body["column"].get<std::string>() : "backlog";
  const auto& cols = kColumns();
  if (std::find(cols.begin(), cols.end(), col) == cols.end()) return json_resp(R"({"error":"invalid_column"})", 422);
  auto c = thread_db().card_create(*id, *title, col);
  return c ? card_json(*c, 201) : not_found_json();
}

Resp api_card_patch(const Req& r) {
  auto id = to_int(get(r.params, "id"));
  if (!id) return not_found_json();
  auto body = json::parse(r.body, nullptr, false);
  if (body.is_discarded() || !body.is_object()) return json_resp(R"({"error":"bad_request"})", 400);
  std::optional<std::string> title, move;
  if (body.contains("title")) {
    if (!body["title"].is_string()) return unprocessable();
    title = valid_title(body["title"].get<std::string>());
    if (!title) return unprocessable();
  }
  if (body.contains("move")) {
    if (!body["move"].is_string()) return json_resp(R"({"error":"bad_request"})", 400);
    auto m = body["move"].get<std::string>();
    if (m != "left" && m != "right") return json_resp(R"({"error":"bad_request"})", 400);
    move = m;
  }
  auto c = thread_db().card_update(*id, title, move);
  return c ? card_json(*c) : not_found_json();
}

Resp api_card_delete(const Req& r) {
  auto id = to_int(get(r.params, "id"));
  if (!id || !thread_db().card_delete(*id)) return not_found_json();
  return Resp{204, "application/json", "", {}};
}

}  // namespace bs::h

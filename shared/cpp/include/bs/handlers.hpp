#pragma once
#include "bs/http.hpp"

namespace bs::h {
Resp healthz(const Req&);
Resp static_file(const Req&);

Resp catalog_page(const Req&);
Resp product_page(const Req&);
Resp cart_page(const Req&);
Resp cart_add_form(const Req&);
Resp cart_item_form(const Req&);
Resp kanban_page(const Req&);
Resp card_create_form(const Req&);
Resp card_update_form(const Req&);
Resp dashboard_page(const Req&);

Resp api_products(const Req&);
Resp api_product(const Req&);
Resp api_cart_get(const Req&);
Resp api_cart_add(const Req&);
Resp api_cart_patch(const Req&);
Resp api_cart_delete(const Req&);
Resp api_boards(const Req&);
Resp api_board(const Req&);
Resp api_card_create(const Req&);
Resp api_card_patch(const Req&);
Resp api_card_delete(const Req&);
Resp api_dashboard(const Req&);
}  // namespace bs::h

namespace bs::detail {
// Shared helpers for handler files.
bool valid_fe(const std::string& fe);
bool is_htmx(const Req& r);
std::optional<int> to_int(const std::string& s);
std::string session_id(const Req& r, Resp& out);  // existing or new (sets cookie)
std::string existing_session(const Req& r);       // "" if none
Resp html(std::string body, int status = 200);
Resp json_resp(const std::string& body, int status = 200);
Resp not_found_html();
Resp not_found_json();
Resp redirect(const std::string& location);
std::string get(const std::map<std::string, std::string>& m, const std::string& k);
}  // namespace bs::detail

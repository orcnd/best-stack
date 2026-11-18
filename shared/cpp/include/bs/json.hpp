#pragma once
#include <nlohmann/json.hpp>

#include "bs/model.hpp"

namespace bs {
using json = nlohmann::json;

void to_json(json& j, const Category& v);
void to_json(json& j, const Product& v);
void to_json(json& j, const ProductPage& v);
void to_json(json& j, const CartItem& v);
void to_json(json& j, const Cart& v);
void to_json(json& j, const Card& v);
void to_json(json& j, const BoardSummary& v);
void to_json(json& j, const Board& v);
void to_json(json& j, const DayPoint& v);
void to_json(json& j, const Dashboard& v);
}  // namespace bs

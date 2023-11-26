#include "quadrant.hpp"

using namespace godot;

bool TileInfo::operator==(const TileInfo &right) const {
  return x == right.x && y == right.y && source_id == right.source_id && alternative_tile_id == right.alternative_tile_id;
}

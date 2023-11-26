#ifndef TILE_MAPPER_QUADRANT
#define TILE_MAPPER_QUADRANT

#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/tile_data.hpp>

namespace godot {

struct CellData;

struct TileInfo {
  int32_t x;
  int32_t y;
  int32_t source_id;
  int32_t alternative_tile_id;

  bool operator==(const TileInfo &right) const;
};

struct Quadrant {
  std::unordered_map<int64_t, CellData*> cells;
  RID canvas_item;
  TileInfo tile_info;
  TileData *tile_data;
};

}

namespace std {

template<>
struct hash<godot::TileInfo> {
  size_t operator()(const godot::TileInfo &tile_info) const noexcept {
    size_t hash_1 = hash<int32_t>{}(tile_info.x);
    size_t hash_2 = hash<int32_t>{}(tile_info.y);
    size_t hash_3 = hash<int32_t>{}(tile_info.source_id);
    size_t hash_4 = hash<int32_t>{}(tile_info.alternative_tile_id);
    size_t hash_1_2 = hash_1 ^ (hash_2 * 2);
    size_t hash_3_4 = hash_3 ^ (hash_4 * 2);
    return hash_1_2 ^ (hash_3_4);
  }
};

}

#endif // !TILE_MAPPER_QUADRANT

#ifndef TILE_MAPPER_CELL
#define TILE_MAPPER_CELL

#include "quadrant.hpp"

#include <functional>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/tile_data.hpp>

namespace godot {

struct CellData {
  int64_t cell_id;
  RID canvas_rid;

  Quadrant *current_quadrant;
  TileInfo tile_info;
  Transform2D transform;
  TileData *tile_data;

  Ref<Texture2D> texture;
  TypedArray<RID> physics_bodies_rid;
};

}

#endif // !TILE_MAPPER_CELL

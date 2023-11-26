#ifndef TILE_MAPPER
#define TILE_MAPPER

#include "godot_cpp/classes/project_settings.hpp"
#include "quadrant.hpp"
#include "cell_data.hpp"

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/physics_server2d.hpp>
#include <godot_cpp/classes/tile_set_atlas_source.hpp>


namespace godot {

struct ShapeData {
  bool one_way;
  real_t margin;
};

class TileMapper : public Node2D {
GDCLASS(TileMapper, Node2D);

private:
  enum CellDrawState {
    CANVAS_ITEM = 0,
    QUADRANT = 1,
    NONE = 1000
  };

  enum CollisionVisibility {
    COLLISION_VISIBILITY_DEFAULT = 0,
    COLLISION_VISIBILITY_ALWAYS = 1,
    COLLISION_VISIBILITY_NONE = 2,
  };

  const Color shape_color = ProjectSettings::get_singleton()->get("debug/shapes/collision/shape_color");

  Ref<TileSet> tile_set;
  PhysicsServer2D::BodyMode collision_type;
  int quadrant_size;
  int collision_visibility;

  int64_t index;
  
  std::unordered_map<TileInfo, std::vector<Quadrant*>> quadrants;
  std::unordered_map<int64_t, CellData*> tiles;

  Ref<TileSetAtlasSource> get_atlas_source(const int32_t source_id) const;
  Ref<Texture2D> get_texture_from_source_id(const int32_t source_id) const;
  Rect2i get_texture_region_from_atlas_source(const int32_t source_id, const Vector2i &atlas_coords) const;
  Rect2i get_texture_region_from_cell_data(CellData *cell_data) const;
  
  RID create_shape_for_cell_layer_polygon_index(CellData *cell_data, const int32_t layer, const int32_t polygon_index) const;
  RID create_cell_body_for_layer(CellData *cell_data, const int32_t layer) const;
  TypedArray<RID> create_physics_bodies_for_cell(CellData *cell_data) const;

  void draw_quadrant(Quadrant *quadrant);
  void draw_quadrant_cell(CellData *cell_data, Quadrant *quadrant);
  bool should_draw_debug_shapes() const;
  void cell_draw_debug_shape(CellData *cell_data, const Color &shape_color);
  void draw_cell_shape(CellData *cell_data, const RID &body, const int shape_index, const Color &shape_color);

  RID get_draw_rid_from_cell_data(CellData *cell_data) const;
  CellDrawState get_cell_draw_state(CellData *cell_data) const;
  bool destroy_quadrant(Quadrant *quadrant);
  void remove_cell(CellData *cell_data, const bool remove_quadrant = true);

  Quadrant *create_new_quadrant() const;
  Quadrant *get_quadrant_with_tile_info(TileInfo tile_info);
  void create_new_cell(const Vector2 &coords, const int32_t source_id, const Vector2i &atlas_coords = Vector2i(), const int alternative_tile_id = 0);

protected:
  static void _bind_methods();
 
public:
  TileMapper();
  ~TileMapper();

  const int64_t INVALID_TILE_ID = 0;

  int64_t add_cell(const Vector2 &coords, const int32_t source_id, const Vector2i &atlas_coords = Vector2i(), const int alternative_tile_id = 0);
  bool destroy_cell(const int64_t cell_id);
  void clear_cells();
  bool is_cell_id_valid(const int64_t cell_id) const;
  PackedInt64Array get_used_tile_ids() const;
  Dictionary get_cell_values(const int64_t cell_id) const;

  void set_tile_set(const Ref<TileSet> new_tile_set);
  Ref<TileSet> get_tile_set() const;

  void set_collision_type(const PhysicsServer2D::BodyMode new_collision_type);
  PhysicsServer2D::BodyMode get_collision_type() const;

  void set_quadrant_size(const int new_quadrant_size);
  int get_quadrant_size() const;

  void set_collision_visibility(const int new_collision_visibility);
  int get_collision_visibility() const;
};

}


#endif // !TILE_MAPPER
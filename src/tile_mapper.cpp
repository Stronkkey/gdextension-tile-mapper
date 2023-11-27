#include "tile_mapper.hpp"

#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/world2d.hpp>
#include <godot_cpp/classes/physics_material.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/scene_tree.hpp>

using namespace godot;

void TileMapper::_bind_methods() {
  ClassDB::bind_method(D_METHOD("add_cell", "coords", "source_id", "atlas_coords", "alternative_tile_id"), &TileMapper::add_cell, DEFVAL(Vector2i()), DEFVAL(0));
  ClassDB::bind_method(D_METHOD("destroy_cell", "cell_id"), &TileMapper::destroy_cell);
  ClassDB::bind_method(D_METHOD("clear_cells"), &TileMapper::clear_cells);
  ClassDB::bind_method(D_METHOD("is_cell_id_valid", "cell_id"), &TileMapper::is_cell_id_valid);
  ClassDB::bind_method(D_METHOD("get_used_tile_ids"), &TileMapper::get_used_tile_ids);
  ClassDB::bind_method(D_METHOD("get_cell_values"), &TileMapper::get_cell_values);

  ClassDB::bind_method(D_METHOD("set_tile_set", "new_tile_set"), &TileMapper::set_tile_set);
  ClassDB::bind_method(D_METHOD("get_tile_set"), &TileMapper::get_tile_set);
  ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tile_set", PROPERTY_HINT_RESOURCE_TYPE, "TileSet"), "set_tile_set", "get_tile_set");

  ClassDB::bind_method(D_METHOD("set_collision_type", "collision_type"), &TileMapper::set_collision_type);
  ClassDB::bind_method(D_METHOD("get_collision_type"), &TileMapper::get_collision_type);
  ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_type", PROPERTY_HINT_ENUM, "Static,Kinematic,Rigid,Rigid Liner"), "set_collision_type", "get_collision_type");

  ClassDB::bind_method(D_METHOD("set_quadrant_size", "new_quadrant_size"), &TileMapper::set_quadrant_size);
  ClassDB::bind_method(D_METHOD("get_quadrant_size"), &TileMapper::get_quadrant_size);
  ADD_PROPERTY(PropertyInfo(Variant::INT, "quadrant_size", PROPERTY_HINT_RANGE, "1,1028,1,or_greater"), "set_quadrant_size", "get_quadrant_size");

  ClassDB::bind_method(D_METHOD("set_collision_visibility", "new_collision_visibility"), &TileMapper::set_collision_visibility);
  ClassDB::bind_method(D_METHOD("get_collision_visibility"), &TileMapper::get_collision_visibility);
  ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_visibility", PROPERTY_HINT_ENUM, "Default,Always,None"), "set_collision_visibility", "get_collision_visibility");
}

TileMapper::TileMapper() {
  collision_type = PhysicsServer2D::BODY_MODE_STATIC;
  quadrant_size = 64;
  collision_visibility = COLLISION_VISIBILITY_DEFAULT;
  index = 1;
  quadrants = {};
  tiles = {};
}

TileMapper::~TileMapper() {
  clear_cells();
}

Ref<TileSetAtlasSource> TileMapper::_get_atlas_source(const int32_t source_id) const {
  if (tile_set.is_null() || !tile_set->has_source(source_id))
    return Ref<TileSetAtlasSource>();
  return Ref<TileSetAtlasSource>(tile_set->get_source(source_id));
}


Ref<Texture2D> TileMapper::_get_texture_from_source_id(const int32_t source_id) const {
  if (tile_set.is_null() || !tile_set->has_source(source_id))
    return Ref<Texture2D>();
  Ref<TileSetAtlasSource> source = _get_atlas_source(source_id);
  return source.is_valid() ? source->get_texture() : Ref<Texture2D>();
}

Rect2i TileMapper::_get_texture_region_from_atlas_source(const int32_t source_id, const Vector2i &atlas_coords) const {
  Ref<TileSetAtlasSource> source = _get_atlas_source(source_id);
  return source.is_valid() ? source->get_tile_texture_region(atlas_coords) : Rect2i();
}

Rect2i TileMapper::_get_texture_region_from_cell_data(CellData *cell_data) const {
return _get_texture_region_from_atlas_source(cell_data->tile_info.source_id, Vector2i(cell_data->tile_info.x, cell_data->tile_info.y));
}


RID TileMapper::_create_shape_for_cell_layer_polygon_index(CellData *cell_data, int32_t layer, int32_t polygon_index) const {
  PackedVector2Array points = cell_data->tile_data->get_collision_polygon_points(layer, polygon_index);
  if (points.size() <= 2)
    return RID();

  RID shape = PhysicsServer2D::get_singleton()->convex_polygon_shape_create();
  PhysicsServer2D::get_singleton()->shape_set_data(shape, points);
  return shape;
}

RID TileMapper::_create_cell_body_for_layer(CellData *cell_data, int32_t layer) const {
  std::vector<RID> shapes = {};
  std::vector<ShapeData> shape_datas = {};

  for (int32_t polygon_index = 0; polygon_index < cell_data->tile_data->get_collision_polygons_count(layer); polygon_index++) {
    RID shape = _create_shape_for_cell_layer_polygon_index(cell_data, layer, polygon_index);
    if (shape == RID())
      continue;

    ShapeData shape_data;
    shape_data.margin = cell_data->tile_data->get_collision_polygon_one_way_margin(layer, polygon_index);
    shape_data.one_way = cell_data->tile_data->is_collision_polygon_one_way(layer, polygon_index);
    shapes.push_back(shape);
    shape_datas.push_back(shape_data);
  }

  if (shapes.empty()) {
    return RID();
  }

  Ref<PhysicsMaterial> material = tile_set->get_physics_layer_physics_material(layer);
  RID body = PhysicsServer2D::get_singleton()->body_create();
  Size2i cell_size = _get_texture_region_from_cell_data(cell_data).get_size();
  Ref<World2D> world_2d = get_world_2d();
  PhysicsServer2D *physics_server2d = PhysicsServer2D::get_singleton();

  physics_server2d->body_set_mode(body, collision_type);
  physics_server2d->body_set_space(body, get_world_2d()->get_space());
  physics_server2d->body_set_state(body, PhysicsServer2D::BODY_STATE_TRANSFORM, cell_data->transform);
  physics_server2d->body_set_collision_layer(body, tile_set->get_physics_layer_collision_layer(layer));
  physics_server2d->body_set_collision_mask(body, tile_set->get_physics_layer_collision_mask(layer));
  physics_server2d->body_set_constant_force(body, cell_data->tile_data->get_constant_linear_velocity(layer));
  physics_server2d->body_set_constant_torque(body, cell_data->tile_data->get_constant_angular_velocity(layer));

  for (int32_t i = 0; i < shapes.size(); i++) {
    RID shape = shapes[i];
    ShapeData shape_data = shape_datas[i];
    physics_server2d->body_add_shape(body, shape, Transform2D(0, -cell_size / 2));
    physics_server2d->body_set_shape_as_one_way_collision(body, i, shape_data.one_way, shape_data.margin);
  }

  if (material.is_valid()) {
    physics_server2d->body_set_param(body, PhysicsServer2D::BODY_PARAM_BOUNCE, material->get_bounce());
    physics_server2d->body_set_param(body, PhysicsServer2D::BODY_PARAM_FRICTION, material->get_friction());
  }
  UtilityFunctions::print("Body");
  return body;
}

TypedArray<RID> TileMapper::_create_physics_bodies_for_cell(CellData *cell_data) const {
  TypedArray<RID> bodies = {};

  for (int32_t layer = 0; layer < tile_set->get_physics_layers_count(); layer++) {
    RID body = _create_cell_body_for_layer(cell_data, layer);
    if (body != RID())
      bodies.append(body);
  }

  return bodies;
}

void TileMapper::_draw_quadrant(Quadrant *quadrant) {
  const bool draw_debug_shapes = _should_draw_debug_shapes();
  RenderingServer::get_singleton()->canvas_item_clear(quadrant->canvas_item);
  for (std::pair<int64_t, CellData*> iterator: quadrant->cells) {
    _draw_quadrant_cell(iterator.second, quadrant);
  }
}

void TileMapper::_draw_quadrant_cell(CellData *cell_data, Quadrant *quadrant) {
  Rect2i size_rect = _get_texture_region_from_cell_data(cell_data);
  Rect2i texture_rect = Rect2i((size_rect.get_position() + cell_data->tile_data->get_texture_origin() - size_rect.size), size_rect.size);

  RenderingServer *rendering_server = RenderingServer::get_singleton();
  rendering_server->canvas_item_add_set_transform(quadrant->canvas_item, cell_data->transform);
  rendering_server->canvas_item_add_texture_rect_region(quadrant->canvas_item,
      texture_rect,
      cell_data->texture->get_rid(),
      size_rect,
      cell_data->tile_data->get_modulate(),
      cell_data->tile_data->get_transpose());

  if (_should_draw_debug_shapes())
    _cell_draw_debug_shape(cell_data, shape_color);
}


Quadrant *TileMapper::_create_new_quadrant() const {
  Quadrant *new_quadrant = memnew(Quadrant);
  Ref<Material> material = get_material();
  new_quadrant->canvas_item = RenderingServer::get_singleton()->canvas_item_create();
  RenderingServer::get_singleton()->canvas_item_set_parent(new_quadrant->canvas_item, get_canvas_item());

  if (material.is_valid())
    RenderingServer::get_singleton()->canvas_item_set_material(new_quadrant->canvas_item, material->get_rid());

  return new_quadrant;
}

bool TileMapper::_should_draw_debug_shapes() const {
  bool debugging_collision_hint = is_inside_tree() && get_tree()->is_debugging_collisions_hint();
  return (debugging_collision_hint && collision_visibility == COLLISION_VISIBILITY_DEFAULT) || collision_visibility == COLLISION_VISIBILITY_ALWAYS;
}

void TileMapper::_cell_draw_debug_shape(CellData *cell_data, const Color &shape_color) {
  for (int i = 0; i < cell_data->physics_bodies_rid.size(); i++) {
    RID body = cell_data->physics_bodies_rid[i];
    int shape_count = PhysicsServer2D::get_singleton()->body_get_shape_count(body);

    for (int shape_index = 0; i < shape_count; i++) {
      _draw_cell_shape(cell_data, body, shape_index, shape_color);
    }
  }
}

void TileMapper::_draw_cell_shape(CellData *cell_data, const RID &body, const int shape_index, const Color &shape_color) {
  Size2i cell_size = _get_texture_region_from_cell_data(cell_data).get_size();
  RID shape = PhysicsServer2D::get_singleton()->body_get_shape(body, shape_index);
  PhysicsServer2D::ShapeType shape_type = PhysicsServer2D::get_singleton()->shape_get_type(shape);
  Transform2D shape_transform = PhysicsServer2D::get_singleton()->body_get_shape_transform(body, shape_index);

  ERR_FAIL_COND_MSG(shape_type != PhysicsServer2D::SHAPE_CONVEX_POLYGON, "Wrong shape type for a tile, should be SHAPE_CONVEX_POLYGON.");
  RID draw_rid = _get_draw_rid_from_cell_data(cell_data);
  ERR_FAIL_COND(draw_rid == RID());

  PackedColorArray colors = {};
  colors.push_back(shape_color);
  RenderingServer::get_singleton()->canvas_item_add_set_transform(draw_rid, cell_data->transform * shape_transform);
  RenderingServer::get_singleton()->canvas_item_add_polygon(draw_rid, PhysicsServer2D::get_singleton()->shape_get_data(shape), colors);
}

void TileMapper::_update_canvas_item_cell(CellData *cell_data) {
  RenderingServer *rendering_server = RenderingServer::get_singleton();
  rendering_server->canvas_item_set_transform(cell_data->canvas_rid, cell_data->transform);
  rendering_server->canvas_item_set_z_index(cell_data->canvas_rid, cell_data->tile_data->get_z_index());
  rendering_server->canvas_item_set_default_texture_filter(cell_data->canvas_rid, static_cast<RenderingServer::CanvasItemTextureFilter>(get_texture_filter()));
  rendering_server->canvas_item_set_default_texture_repeat(cell_data->canvas_rid, static_cast<RenderingServer::CanvasItemTextureRepeat>(get_texture_repeat()));
  rendering_server->canvas_item_set_light_mask(cell_data->canvas_rid, get_light_mask());
}

void TileMapper::_draw_tile(CellData *cell_data) {
  RenderingServer *rendering_server = RenderingServer::get_singleton();
  Rect2i size_rect = _get_texture_region_from_cell_data(cell_data);
  Rect2i texture_rect = Rect2i((size_rect.position + cell_data->tile_data->get_texture_origin()), size_rect.get_size());

  rendering_server->canvas_item_clear(cell_data->canvas_rid);
  rendering_server->canvas_item_add_texture_rect_region(cell_data->canvas_rid, 
      texture_rect,
      cell_data->texture->get_rid(),
      size_rect);
  rendering_server->canvas_item_set_parent(cell_data->canvas_rid, get_canvas_item());

  Ref<Material> material = cell_data->tile_data->get_material();
  
  if (material.is_valid())
    rendering_server->canvas_item_set_material(cell_data->canvas_rid, material->get_rid());

  if (_should_draw_debug_shapes())
    _cell_draw_debug_shape(cell_data, shape_color);
}

void TileMapper::_set_cell_to_use_canvas_item_cell(CellData *cell_data) {
  cell_data->canvas_rid = RenderingServer::get_singleton()->canvas_item_create();
  if (cell_data->current_quadrant) {
    auto iterator = cell_data->current_quadrant->cells.find(cell_data->cell_id);
    if (iterator != cell_data->current_quadrant->cells.end())
      cell_data->current_quadrant->cells.erase(iterator);
  }
  _draw_quadrant(cell_data->current_quadrant);
  cell_data->current_quadrant = nullptr;
  _draw_tile(cell_data);
  _update_canvas_item_cell(cell_data);
}

void TileMapper::_set_cell_transform(CellData *cell_data, const Transform2D &new_transform) {
  cell_data->transform = new_transform;

  switch (_get_cell_draw_state(cell_data)) {
    CANVAS_ITEM:
      RenderingServer::get_singleton()->canvas_item_set_transform(cell_data->canvas_rid, new_transform);
      break;
    QUADRANT:
      _draw_quadrant(cell_data->current_quadrant);
      break;
    default:
      break;
  }

  for (int64_t i = 0; i < cell_data->physics_bodies_rid.size(); i++)
    PhysicsServer2D::get_singleton()->body_set_state(cell_data->physics_bodies_rid[i], PhysicsServer2D::BODY_STATE_TRANSFORM, cell_data->transform);
}

void TileMapper::_set_cell_to_use_quadrant(CellData *cell_data, Quadrant *quadrant) {
  if (cell_data->canvas_rid != RID())
    RenderingServer::get_singleton()->free_rid(cell_data->canvas_rid);
  cell_data->canvas_rid = RID();
  quadrant->cells.insert({cell_data->cell_id, cell_data});
  _draw_quadrant_cell(cell_data, quadrant);
}

RID TileMapper::_get_draw_rid_from_cell_data(CellData *cell_data) const {
  CellDrawState cell_draw_state = _get_cell_draw_state(cell_data);
  if (cell_draw_state == NONE) {
    ERR_PRINT("Invalid Cell");
    return RID();
  }

  RID quadrant_rid = cell_data->current_quadrant != nullptr ? cell_data->current_quadrant->canvas_item : RID();
  return cell_draw_state == CANVAS_ITEM ? cell_data->canvas_rid : quadrant_rid; 
}

TileMapper::CellDrawState TileMapper::_get_cell_draw_state(CellData *cell_data) const {
  if (cell_data != nullptr && cell_data->canvas_rid != RID())
    return CANVAS_ITEM;
  return cell_data->current_quadrant != nullptr ? QUADRANT : NONE;
}

void TileMapper::_destroy_quadrant(Quadrant *quadrant) {
  std::vector<Quadrant*> tile_info_quadrants = {};
  auto iterator = quadrants.find(quadrant->tile_info);
  if (iterator != quadrants.end()) {
    tile_info_quadrants = iterator->second;
  }

  if (!tile_info_quadrants.empty()) {
    auto quadrant_iterator = std::find(tile_info_quadrants.begin(), tile_info_quadrants.end(), quadrant);
    if (quadrant_iterator != tile_info_quadrants.end()) {
      tile_info_quadrants.erase(quadrant_iterator);
      quadrants.insert_or_assign(quadrant->tile_info, tile_info_quadrants);
    }
  }

  RenderingServer::get_singleton()->canvas_item_clear(quadrant->canvas_item);
  RenderingServer::get_singleton()->free_rid(quadrant->canvas_item);
  memdelete(quadrant);
}

void TileMapper::_destroy_quadrant_cells(Quadrant *quadrant) {
  std::unordered_map<int64_t, CellData*> quadrant_cells = quadrant->cells;
  for (std::pair<int64_t, CellData*> iterator: quadrant_cells) {
    auto cell_iterator = tiles.find(iterator.first);
    if (cell_iterator != tiles.end())
      tiles.erase(cell_iterator);

    _remove_cell(iterator.second, false);
  }

  quadrant->cells.clear();
}

void TileMapper::_remove_cell(CellData *cell_data, const bool remove_quadrant) {
  Quadrant *cell_quadrant = cell_data->current_quadrant;
  PhysicsServer2D *physics_server2d = PhysicsServer2D::get_singleton();
  RenderingServer *rendering_server = RenderingServer::get_singleton();

  if (cell_data->canvas_rid != RID()) {
    rendering_server->free_rid(cell_data->canvas_rid);
  }

  if (remove_quadrant && cell_quadrant != nullptr) {
    auto quadrant_cells_iterator = cell_quadrant->cells.find(cell_data->cell_id);
    if (quadrant_cells_iterator != cell_quadrant->cells.end())
      cell_quadrant->cells.erase(cell_data->cell_id);
  }

  int64_t physics_bodies_rid_size = cell_data->physics_bodies_rid.size();

  for (int i = 0; i < physics_bodies_rid_size; i++) {
    RID body = cell_data->physics_bodies_rid[i];
    int32_t shape_count = physics_server2d->body_get_shape_count(body);

    for (int shape_index; i < shape_count; i++) {
      physics_server2d->free_rid(physics_server2d->body_get_shape(body, shape_index));
    }

    physics_server2d->free_rid(body);
  }

  memdelete(cell_data);
}

Quadrant *TileMapper::_get_quadrant_with_tile_info(TileInfo tile_info) {
  std::vector<Quadrant*> cell_quadrants = {};
  auto iterator = quadrants.find(tile_info);

  if (iterator != quadrants.end()) {
    cell_quadrants = iterator->second;
  }

  if (!cell_quadrants.empty()) {
    Quadrant *last_quadrant = cell_quadrants.back();
    bool new_quadrant = last_quadrant->cells.size() >= quadrant_size;
    Quadrant *quadrant = last_quadrant->cells.size() >= quadrant_size ? _create_new_quadrant() : last_quadrant;
    Ref<TileSetAtlasSource> source = _get_atlas_source(tile_info.source_id);
  
    if (last_quadrant->cells.size() <= quadrant_size && source.is_valid()) {
      quadrant->tile_info = tile_info;
      quadrant->tile_data = source->get_tile_data(Vector2i(tile_info.x, tile_info.y), tile_info.alternative_tile_id);
      RenderingServer::get_singleton()->canvas_item_set_z_index(quadrant->canvas_item, quadrant->tile_data->get_z_index());
      Ref<Material> material = quadrant->tile_data->get_material();
      if (material.is_valid())
        RenderingServer::get_singleton()->canvas_item_set_material(quadrant->canvas_item, material->get_rid());
    }

    if (new_quadrant) {
      std::vector<Quadrant*> new_cell_quadrants = cell_quadrants;
      new_cell_quadrants.push_back(quadrant);
      quadrants.insert_or_assign(tile_info, new_cell_quadrants);
    }
    return quadrant;
  }

  Quadrant *new_quadrant = _create_new_quadrant();
  std::vector<Quadrant*> new_quadrant_vector = {new_quadrant};
  quadrants.insert({tile_info, new_quadrant_vector});
  return new_quadrant;
}

void TileMapper::_create_new_cell(const Vector2 &coords, const int32_t source_id, const Vector2i &atlas_coords, const int alternative_tile_id) {
  ERR_FAIL_COND_MSG(tile_set.is_null(), "Tried adding cell with no TileSet.");
  ERR_FAIL_COND_MSG(!tile_set->has_source(source_id), vformat("TileSet source with id %s does not exist.", source_id));
  Ref<TileSetAtlasSource> source = _get_atlas_source(source_id);

  ERR_FAIL_COND_MSG(source.is_null(), vformat("Tile source with id %s is not a TileSetAtlasSource.", source_id));
  ERR_FAIL_COND_MSG(!source->has_tile(atlas_coords), vformat("No tile at %s.", atlas_coords));
  ERR_FAIL_COND_MSG(!source->has_alternative_tile(atlas_coords, alternative_tile_id), vformat(""));

  CellData *cell_data = memnew(CellData);
  int64_t cell_id = index;
  TileInfo tile_info;
  tile_info.x = atlas_coords.x;
  tile_info.y = atlas_coords.y;
  tile_info.source_id = source_id;
  tile_info.alternative_tile_id = alternative_tile_id;

  cell_data->cell_id = cell_id;
  cell_data->texture = _get_texture_from_source_id(source_id);
  cell_data->tile_info = tile_info;
  cell_data->transform = Transform2D(0, coords);
  cell_data->tile_data = source->get_tile_data(atlas_coords, alternative_tile_id);
  cell_data->physics_bodies_rid = _create_physics_bodies_for_cell(cell_data);

  Quadrant *quadrant = _get_quadrant_with_tile_info(tile_info);

  quadrant->cells.insert({cell_id, cell_data});
  tiles.insert({cell_id, cell_data});

  cell_data->current_quadrant = quadrant;
  _draw_quadrant_cell(cell_data, quadrant);
}


int64_t TileMapper::add_cell(const Vector2 &coords, const int32_t source_id, const Vector2i &atlas_coords, const int alternative_tile_id) {
  if (index == INVALID_TILE_ID)
    index++;

  int64_t cell_id = index;
  _create_new_cell(coords, source_id, atlas_coords, alternative_tile_id);
  
  index++;
  return cell_id;
}

bool TileMapper::destroy_cell(const int64_t cell_id) {
  auto iterator = tiles.find(cell_id);
  if (iterator != tiles.end()) {
    CellData *cell_data = iterator->second;
    Quadrant *quadrant = cell_data->current_quadrant;

    if (quadrant != nullptr && quadrant->cells.size() == 1)
      _destroy_quadrant(quadrant);

    _remove_cell(cell_data);
    return true;
  }

  return false;
}

void TileMapper::clear_cells() {
  std::unordered_map<TileInfo, std::vector<Quadrant*>> local_quadrants = quadrants;
  std::unordered_map<int64_t, CellData*> local_tiles = tiles;
  
  for (auto iterator: local_quadrants) {
    std::vector<Quadrant*> local_local_quadrants = iterator.second;
    for (Quadrant *quadrant: local_local_quadrants) {
      _destroy_quadrant_cells(quadrant);
      _destroy_quadrant(quadrant);
    }
  }

  tiles.clear();
  quadrants.clear();
  UtilityFunctions::print(tiles.size());
  UtilityFunctions::print(quadrants.size());
}

bool TileMapper::is_cell_id_valid(const int64_t cell_id) const {
  return tiles.find(cell_id) != tiles.end();
}

PackedInt64Array TileMapper::get_used_tile_ids() const {
  PackedInt64Array tile_ids = {};
  int64_t i = 0;
  tile_ids.resize(tiles.size());
  
  for (const std::pair<int64_t, CellData*> iterator: tiles) {
    tile_ids.set(i, iterator.first);
    i++;
  }

  tile_ids.sort();
  return tile_ids;
}

Dictionary TileMapper::get_cell_values(const int64_t cell_id) const {
  Dictionary data = {};

  auto iterator = tiles.find(cell_id);
  CellData *cell_data;
  if (iterator == tiles.end())
    return data;

  cell_data = iterator->second;
  data["canvas_rid"] = cell_data->canvas_rid;
  data["tranform"] = cell_data->transform;
  data["tile_data"] = cell_data->tile_data;
  data["texture"] = cell_data->texture;
  data["physics_bodies_rid"] = cell_data->physics_bodies_rid;

  return data;
}


void TileMapper::set_tile_set(Ref<TileSet> new_tile_set) {
  tile_set = new_tile_set;
}

Ref<TileSet> TileMapper::get_tile_set() const {
  return tile_set;
}

void TileMapper::set_collision_type(const PhysicsServer2D::BodyMode new_collision_type) {
  collision_type = new_collision_type;
}

PhysicsServer2D::BodyMode TileMapper::get_collision_type() const {
  return collision_type;
}

void TileMapper::set_quadrant_size(const int new_quadrant_size) {
  quadrant_size = new_quadrant_size;
}

int TileMapper::get_quadrant_size() const {
  return quadrant_size;
}

void TileMapper::set_collision_visibility(const int new_collision_visibility) {
  collision_visibility = new_collision_visibility;
}

int TileMapper::get_collision_visibility() const {
  return collision_visibility;
}

// Copyright <disenone>

#include <cassert>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <utility>
#include <boost/range/irange.hpp>

#include "cross.hpp"

namespace aoi { namespace cross {

#define MOVE_DIRECTION_LEFT 0
#define MOVE_DIRECTION_RIGHT 1

#define IfInXZRadiusSquare(dx, dz, x0, z0, x1, z1, radius_square) \
  dx = x0 - x1; \
  dz = z0 - z1; \
  if (dx * dx + dz * dz <= radius_square) \

#define IfNotInXZRadiusSquare(dx, dz, x0, z0, x1, z1, radius_square) \
  dx = x0 - x1; \
  dz = z0 - z1; \
  if (dx * dx + dz * dz > radius_square) \

//--------------------------------------------------------------------------------------------------
class KHashDeleter {
 public:
  void operator()(khash_t(SensorHashMap) *ptr) {
    kh_destroy(SensorHashMap, ptr);
  }
};

Sensor::Sensor(Nuid _sensor_id, float _radius, PlayerAoi *_pplayer)
    : sensor_id(_sensor_id), radius(_radius),
      radius_square(_radius * _radius), pplayer(_pplayer),
      left_x(COORD_TYPE_GUARD_LEFT, _pplayer->pos.x - _radius, _pplayer, this),
      right_x(COORD_TYPE_GUARD_RIGHT, _pplayer->pos.x + _radius, _pplayer, this),
      left_z(COORD_TYPE_GUARD_LEFT, _pplayer->pos.z - radius, _pplayer, this),
      right_z(COORD_TYPE_GUARD_RIGHT, _pplayer->pos.z + radius, _pplayer, this),
      aoi_player_candidates(kh_init(SensorHashMap), KHashDeleter()) {
    kh_resize(SensorHashMap, aoi_player_candidates.get(), 100);
  }

inline void Sensor::AddCandidate(PlayerAoi* other_pplayer) {
  if (pplayer->nuid == other_pplayer->nuid) return;

  auto candidates = aoi_player_candidates.get();
  if (kh_get(SensorHashMap, candidates, other_pplayer->nuid) == kh_end(candidates)) {
    int ret;
    auto k = kh_put(SensorHashMap, candidates, other_pplayer->nuid, &ret);
    assert(ret != -1);
    kh_value(candidates, k) = other_pplayer;

    if (other_pplayer->detected_by) {
      (*other_pplayer->detected_by)[pplayer->nuid].push_back(sensor_id);
    }
  }
}

inline void Sensor::RemoveCandidate(PlayerAoi* other_pplayer) {
  auto candidates = aoi_player_candidates.get();
  auto k = kh_get(SensorHashMap, candidates, other_pplayer->nuid);
  if (k != kh_end(candidates)) {
    kh_del(SensorHashMap, candidates, k);

    if (other_pplayer->detected_by) {
      auto &sensor_ids = (*other_pplayer->detected_by)[pplayer->nuid];
      sensor_ids.erase(std::find(sensor_ids.begin(), sensor_ids.end(), sensor_id));
    }
  }
}

inline void Sensor::PrintCandidate() {
  printf("Player %lu Sensor %lu candidates: ", pplayer->nuid, sensor_id);
  PlayerAoi *val;
  kh_foreach_value(aoi_player_candidates.get(), val,
    printf("%lu ", val->nuid);
  )
  printf("\n");
}
//--------------------------------------------------------------------------------------------------
void CoordNode::PrintLog() {
  printf("(");
  printf("%p, ", this);
  switch (type) {
    case COORD_TYPE_PLAYER:
      printf("player");
      break;
    case COORD_TYPE_GUARD_LEFT:
      printf("left");
      break;
    case COORD_TYPE_GUARD_RIGHT:
      printf("right");
      break;
  }
  printf(" %lu, %f", pplayer->nuid, value);
  printf("), ");
}

//--------------------------------------------------------------------------------------------------
inline void ListInsertBefore(CoordNode **list, CoordNode *pos, CoordNode *ptr) {
  if (!(*list)) {
    ptr->next = nullptr;
    ptr->prev = nullptr;
    *list = ptr;
  } else {
    if (pos->prev) {
      pos->prev->next = ptr;
    }
    ptr->prev = pos->prev;
    pos->prev = ptr;
    ptr->next = pos;
    if (*list == pos) *list = ptr;
  }
}

//--------------------------------------------------------------------------------------------------
inline void ListInsertAfter(CoordNode **list, CoordNode *pos, CoordNode *ptr) {
  if (!(*list)) {
    ptr->next = nullptr;
    ptr->prev = nullptr;
    *list = ptr;
  } else {
    if (pos->next) {
      pos->next->prev = ptr;
    }
    ptr->next = pos->next;
    pos->next = ptr;
    ptr->prev = pos;
  }
}

//--------------------------------------------------------------------------------------------------
inline void ListRemove(CoordNode **list, CoordNode *pos) {
  if (!pos->prev && !pos->next) {
    *list = nullptr;
  } else {
    if (pos->prev) {
      pos->prev->next = pos->next;
    }
    if (pos->next) {
      pos->next->prev = pos->prev;
    }
    if (*list == pos) *list = pos->next;
  }
}

//--------------------------------------------------------------------------------------------------
inline void MoveIn(CoordNode *player_node, CoordNode *sensor_node) {
  if (player_node->pplayer->nuid == sensor_node->pplayer->nuid) return;
  const auto &pos = player_node->pplayer->pos;
  const auto &other_pos = sensor_node->pplayer->pos;
  auto radius = sensor_node->psensor->radius;

  if (fabs(pos.x - other_pos.x) < radius && fabs(pos.z - other_pos.z) < radius) {
    sensor_node->psensor->AddCandidate(player_node->pplayer);
  }
}

inline void MoveOut(CoordNode *player_node, CoordNode *sensor_node) {
  sensor_node->psensor->RemoveCandidate(player_node->pplayer);
}

#define MOVE_CROSS_ID(dir, type1, type2) ((dir << 16) + (type1 << 8) + type2)

inline void MoveCross(Uint32 dir, CoordNode *moving_node, CoordNode *static_node) {
  Uint32 cross_id = MOVE_CROSS_ID(dir, (Uint32)moving_node->type, (Uint32)static_node->type);

  switch (cross_id) {
    case MOVE_CROSS_ID(MOVE_DIRECTION_LEFT, COORD_TYPE_PLAYER, COORD_TYPE_GUARD_RIGHT):
    case MOVE_CROSS_ID(MOVE_DIRECTION_RIGHT, COORD_TYPE_PLAYER, COORD_TYPE_GUARD_LEFT):
      MoveIn(moving_node, static_node);
      break;

    case MOVE_CROSS_ID(MOVE_DIRECTION_LEFT, COORD_TYPE_GUARD_LEFT, COORD_TYPE_PLAYER):
    case MOVE_CROSS_ID(MOVE_DIRECTION_RIGHT, COORD_TYPE_GUARD_RIGHT, COORD_TYPE_PLAYER):
      MoveIn(static_node, moving_node);
      break;

    case MOVE_CROSS_ID(MOVE_DIRECTION_LEFT, COORD_TYPE_PLAYER, COORD_TYPE_GUARD_LEFT):
    case MOVE_CROSS_ID(MOVE_DIRECTION_RIGHT, COORD_TYPE_PLAYER, COORD_TYPE_GUARD_RIGHT):
      MoveOut(moving_node, static_node);
      break;

    case MOVE_CROSS_ID(MOVE_DIRECTION_LEFT, COORD_TYPE_GUARD_RIGHT, COORD_TYPE_PLAYER):
    case MOVE_CROSS_ID(MOVE_DIRECTION_RIGHT, COORD_TYPE_GUARD_LEFT, COORD_TYPE_PLAYER):
      MoveOut(static_node, moving_node);
      break;
  }
}

//--------------------------------------------------------------------------------------------------
void ListUpdateNode(CoordNode **list, CoordNode *pnode) {
  float value = pnode->value;

  if (pnode->next && pnode->next->value < value) {
    // move right
    auto cur_node = pnode->next;
    while (1) {
      MoveCross(MOVE_DIRECTION_RIGHT, pnode, cur_node);
      if (!cur_node->next || cur_node->next->value >= value) break;
      cur_node = cur_node->next;
    }

    ListRemove(list, pnode);
    ListInsertAfter(list, cur_node, pnode);

  } else if (pnode->prev && pnode->prev->value > value) {
    // move left
    auto cur_node = pnode->prev;
    while (1) {
      MoveCross(MOVE_DIRECTION_LEFT, pnode, cur_node);
      if (!cur_node->prev || cur_node->prev->value <= value) break;
      cur_node = cur_node->prev;
    }

    ListRemove(list, pnode);
    ListInsertBefore(list, cur_node, pnode);
  }
}

//--------------------------------------------------------------------------------------------------
CrossAoi::CrossAoi(float map_bound_xmin, float map_bound_xmax, float map_bound_zmin,
                   float map_bound_zmax, size_t beacon_x, size_t beacon_z, float beacon_radius) {
  if (beacon_x == 0 && beacon_z == 0) return;

  assert(map_bound_xmax > map_bound_xmin);
  assert(map_bound_zmax > map_bound_zmin);
  float step_x = (map_bound_xmax - map_bound_xmin) / beacon_x / 2;
  float step_z = (map_bound_zmax - map_bound_zmin) / beacon_z / 2;
  for (auto x : boost::irange(beacon_x)) {
    for (auto z : boost::irange(beacon_z)) {
      float pos_x = map_bound_xmin + step_x * (x * 2 + 1);
      float pos_z = map_bound_zmin + step_z * (z * 2 + 1);
      auto nuid = GenNuid();
      AddPlayerNoBeacon(nuid, pos_x, 0, pos_z);
      AddSensorNoBeacon(nuid, GenNuid(), beacon_radius);
      auto &beacon = *player_map_[nuid];
      beacon.SetFlag_Beacon();
      beacon.detected_by.reset(new boost::unordered_map<Nuid, std::vector<Nuid>>());
      beacons.push_back(&beacon);
    }
  }
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::AddPlayer(Nuid nuid, float x, float y, float z) {
  if (beacons.empty()) {
    AddPlayerNoBeacon(nuid, x, y, z);
    return;
  }

  auto piter = player_map_.find(nuid);
  if (piter != player_map_.end()) {
    AddPlayerNoBeacon(nuid, x, y, z);
    return;
  }

  auto ret = player_map_.emplace(nuid, new PlayerAoi(nuid, x, y, z));
  auto &player = *ret.first->second;
  player.SetFlag_New();

  // 找最近的 beacon
  PlayerAoi* best_beacon = nullptr;
  float min_dist = std::numeric_limits<float>::max();
  for (auto pbeacon : beacons) {
    float dx = pbeacon->pos.x - x;
    float dz = pbeacon->pos.z - z;
    float dist = dx * dx + dz * dz;
    if (dist < min_dist) {
      min_dist = dist;
      best_beacon = pbeacon;
    }
  }
  assert(best_beacon);

  // 复制 detected_by
  for (auto &elem : *best_beacon->detected_by) {
    auto other_nuid = elem.first;
    auto &other_player = *player_map_[other_nuid];
    for (auto sensor_id : elem.second) {
      for (auto &sensor : other_player.sensors) {
        if (sensor.sensor_id == sensor_id) {
          sensor.AddCandidate(&player);
        }
      }
    }
  }
  if (!best_beacon->sensors.empty()) {
    for (auto &sensor : best_beacon->sensors) {
      sensor.AddCandidate(&player);
    }
  }

  ListInsertBefore(&coord_list_x_, &best_beacon->node_x, &player.node_x);
  ListInsertBefore(&coord_list_z_, &best_beacon->node_z, &player.node_z);
  UpdatePos(nuid, x, y, z);
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::AddPlayerNoBeacon(Nuid nuid, float x, float y, float z) {
  auto piter = player_map_.find(nuid);

  if (piter == player_map_.end()) {
    auto ret = player_map_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(nuid),
      std::forward_as_tuple(new PlayerAoi(nuid, x, y, z)));
    auto &player = *ret.first->second;
    player.SetFlag_New();
    ListInsertBefore(&coord_list_x_, coord_list_x_, &player.node_x);
    ListInsertBefore(&coord_list_z_, coord_list_z_, &player.node_z);
  } else {
    piter->second->UnsetFlag_Removed();
  }
  UpdatePos(nuid, x, y, z);
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::RemovePlayer(Nuid nuid) {
  auto piter = player_map_.find(nuid);
  if (piter == player_map_.end()) return;

  auto &player = *piter->second;
  player.SetFlag_Removed();
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::_RemovePlayer(Nuid nuid) {
  auto piter = player_map_.find(nuid);
  if (piter == player_map_.end()) return;

  auto &player = *piter->second;
  std::vector<Nuid> sensor_ids;
  for (auto siter = player.sensors.rbegin(); siter != player.sensors.rend(); ++siter) {
    sensor_ids.push_back(siter->sensor_id);
  }

  for (auto sensor_id : sensor_ids) {
    RemoveSensor(nuid, sensor_id);
  }

  ListRemove(&coord_list_x_, &player.node_x);
  ListRemove(&coord_list_z_, &player.node_z);
  player_map_.erase(nuid);
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::AddSensor(Nuid nuid, Nuid sensor_id, float radius) {
  if (beacons.empty()) {
    return AddSensorNoBeacon(nuid, sensor_id, radius);
  }

  auto piter = player_map_.find(nuid);
  if (piter == player_map_.end()) return;

  auto &player = *piter->second;
  // 找最近的 beacon
  PlayerAoi* best_beacon = nullptr;
  float min_dist = std::numeric_limits<float>::max();
  for (auto pbeacon : beacons) {
    float dx = pbeacon->pos.x - player.pos.x;
    float dz = pbeacon->pos.z - player.pos.z;
    float dist = dx * dx + dz * dz;
    if (dist < min_dist) {
      min_dist = dist;
      best_beacon = pbeacon;
    }
  }
  assert(best_beacon);

  auto &best_sensor = best_beacon->sensors[0];
  float dr = best_sensor.radius - radius;
  if (dr * dr + min_dist > radius) {
    return AddSensorNoBeacon(nuid, sensor_id, radius);
  }

  player.sensors.emplace_back(sensor_id, radius, &player);
  auto &sensor = player.sensors[player.sensors.size() - 1];
  auto size = kh_size(best_sensor.aoi_player_candidates.get());
  kh_resize(SensorHashMap, sensor.aoi_player_candidates.get(), size);
  PlayerAoi *val;
  kh_foreach_value(best_sensor.aoi_player_candidates.get(), val,
    sensor.AddCandidate(val);
  )
  sensor.AddCandidate(best_beacon);

  ListInsertBefore(&coord_list_x_, &best_sensor.left_x, &sensor.left_x);
  ListInsertAfter(&coord_list_x_, &best_sensor.right_x, &sensor.right_x);

  ListInsertBefore(&coord_list_z_, &best_sensor.left_z, &sensor.left_z);
  ListInsertAfter(&coord_list_z_, &best_sensor.right_z, &sensor.right_z);

  UpdateSensorPos(player, &sensor);
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::AddSensorNoBeacon(Nuid nuid, Nuid sensor_id, float radius) {
  auto piter = player_map_.find(nuid);
  if (piter == player_map_.end()) return;

  auto &player = *piter->second;
  player.sensors.emplace_back(sensor_id, radius, &player);
  auto &sensor = player.sensors[player.sensors.size() - 1];

  ListInsertBefore(&coord_list_x_, &player.node_x, &sensor.left_x);
  ListInsertAfter(&coord_list_x_, &player.node_x, &sensor.right_x);

  ListInsertBefore(&coord_list_z_, &player.node_z, &sensor.left_z);
  ListInsertAfter(&coord_list_z_, &player.node_z, &sensor.right_z);

  UpdateSensorPos(player, &sensor);
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::RemoveSensor(Nuid nuid, Nuid sensor_id) {
  auto piter = player_map_.find(nuid);
  if (piter == player_map_.end()) return;

  auto &player = *piter->second;
  if (player.sensors.size() > 1) {
    auto &last_sensor = player.sensors[player.sensors.size() - 1];
    if (last_sensor.sensor_id != sensor_id) {
      for (auto &sensor : player.sensors) {
        if (sensor.sensor_id == sensor_id) {
          std::swap(sensor, last_sensor);
          break;
        }
      }
    }
  }

  auto &last_sensor = player.sensors[player.sensors.size() - 1];
  if (last_sensor.sensor_id != sensor_id) return;

  ListRemove(&coord_list_x_, &last_sensor.left_x);
  ListRemove(&coord_list_x_, &last_sensor.right_x);
  ListRemove(&coord_list_z_, &last_sensor.left_z);
  ListRemove(&coord_list_z_, &last_sensor.right_z);
  player.sensors.pop_back();
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::UpdatePos(Nuid nuid, float x, float y, float z) {
  auto piter = player_map_.find(nuid);
  if (piter == player_map_.end()) return;

  auto &player = *piter->second;
  player.pos.Set(x, y, z);
  player.SetFlag_Dirty();

  player.node_x.value = player.pos.x;
  ListUpdateNode(&coord_list_x_, &player.node_x);

  player.node_z.value = player.pos.z;
  ListUpdateNode(&coord_list_z_, &player.node_z);

  if (!player.sensors.empty()) {
    for (auto &sensor : player.sensors) {
      UpdateSensorPos(player, &sensor);
    }
  }
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::UpdateSensorPos(const PlayerAoi &player, Sensor *psensor) {
  auto radius = psensor->radius;
  psensor->right_x.value = player.pos.x + radius;
  ListUpdateNode(&coord_list_x_, &psensor->right_x);

  psensor->left_x.value = player.pos.x - radius;
  ListUpdateNode(&coord_list_x_, &psensor->left_x);

  psensor->right_z.value = player.pos.z + radius;
  ListUpdateNode(&coord_list_z_, &psensor->right_z);

  psensor->left_z.value = player.pos.z - radius;
  ListUpdateNode(&coord_list_z_, &psensor->left_z);
}

//--------------------------------------------------------------------------------------------------
AoiUpdateInfos CrossAoi::Tick() {
  // 全量做一遍 aoi
  AoiUpdateInfos update_infos;
  PlayerPtrList remove_list;

  for (auto& elem : player_map_) {
    auto& player = *elem.second;
    if (player.GetFlag_Beacon()) continue;

    if (player.GetFlag_Removed()) {
      remove_list.push_back(&player);
      continue;
    }

    if (!player.sensors.empty()) {
      auto update_info = _UpdatePlayerAoi(cur_aoi_map_idx_, &player);
      if (!update_info.sensor_update_list.empty()) {
        update_infos.emplace(update_info.nuid, std::move(update_info));
      }
    }

    player.UnsetFlag_New();
  }

  for (auto pptr : remove_list) {
    _RemovePlayer(pptr->nuid);
  }

  for (auto& elem : player_map_) {
    auto& player = *elem.second;
    player.last_pos = player.pos;
  }
  cur_aoi_map_idx_ = 1 - cur_aoi_map_idx_;
  return update_infos;
}

//--------------------------------------------------------------------------------------------------
AoiUpdateInfo CrossAoi::_UpdatePlayerAoi(Uint32 cur_aoi_map_idx,
                                          PlayerAoi* pptr) {
  AoiUpdateInfo aoi_update_info;
  aoi_update_info.nuid = pptr->nuid;
  Uint32 new_aoi_map_idx = 1 - cur_aoi_map_idx;

  for (auto& sensor : pptr->sensors) {
    auto& old_aoi = sensor.aoi_players[cur_aoi_map_idx];
    auto& new_aoi = sensor.aoi_players[new_aoi_map_idx];
    _CalcAoiPlayers(*pptr, sensor, &new_aoi);

    SensorUpdateInfo update_info;

    auto& enters = update_info.enters;
    auto& leaves = update_info.leaves;
    float radius_square = sensor.radius_square;

    _CheckLeave(pptr, radius_square, old_aoi, &leaves);
    _CheckEnter(pptr, radius_square, new_aoi, &enters);

    if (enters.empty() && leaves.empty()) {
      continue;
    }

    update_info.sensor_id = sensor.sensor_id;
    aoi_update_info.sensor_update_list.push_back(std::move(update_info));
  }

  return aoi_update_info;
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::_CalcAoiPlayers(const PlayerAoi& player, const Sensor& sensor,
                                PlayerPtrList* aoi_map) {
  aoi_map->clear();
  auto candidates = sensor.aoi_player_candidates.get();
  aoi_map->reserve(kh_size(candidates));

  auto pos = player.pos;
  auto radius = sensor.radius;
  auto radius_suqare = radius * radius;
  float dx, dz;

  PlayerAoi* other_ptr;
  std::vector<PlayerAoi*> remove_players;
  kh_foreach_value(candidates, other_ptr,
    if (other_ptr->GetFlag_Beacon()) continue;
    if (other_ptr->GetFlag_Removed()) {
      remove_players.push_back(other_ptr);
      continue;
    }
    IfInXZRadiusSquare(dx, dz, other_ptr->pos.x, other_ptr->pos.z, pos.x, pos.z, radius_suqare) {
      aoi_map->emplace_back(other_ptr);
    }
  )
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::_CheckLeave(PlayerAoi* pptr, float radius_square,
                             const PlayerPtrList &aoi_players, PlayerNuids *leaves) {
  const auto &player_pos = pptr->pos;
  float dx, dz;
  float pos_x = player_pos.x;
  float pos_z = player_pos.z;
  for (auto old_player_ptr : aoi_players) {
    if (old_player_ptr->GetFlag_Removed()) {
      leaves->push_back(old_player_ptr->nuid);
    } else {
      IfNotInXZRadiusSquare(dx, dz, old_player_ptr->pos.x, old_player_ptr->pos.z,
                            pos_x, pos_z, radius_square) {
        leaves->push_back(old_player_ptr->nuid);
      }
    }
  }
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::_CheckEnter(PlayerAoi* pptr, float radius_square,
                             const PlayerPtrList &aoi_players, PlayerNuids *enters) {
  const auto &player_last_pos = pptr->last_pos;
  float pos_x = player_last_pos.x;
  float pos_z = player_last_pos.z;

  if (pptr->GetFlag_New()) {
    enters->reserve(aoi_players.size());
    for (auto new_player_ptr : aoi_players) {
      enters->push_back(new_player_ptr->nuid);
    }
    return;
  }

  float dx, dz;
  for (auto new_player_ptr : aoi_players) {
    IfNotInXZRadiusSquare(dx, dz, new_player_ptr->last_pos.x, new_player_ptr->last_pos.z,
                          pos_x, pos_z, radius_square) {
      enters->push_back(new_player_ptr->nuid);
    }
  }
}

//--------------------------------------------------------------------------------------------------
void CrossAoi::_PrintNodeList(CoordNode *list) {
  printf("[");
  auto cur_node = list;
  while (cur_node) {
    cur_node->PrintLog();
    cur_node = cur_node->next;
  }
  printf("]");
}


void CrossAoi::PrintAllNodeList() {
  printf("x_list: ");
  _PrintNodeList(coord_list_x_);
  printf("\nz_list: ");
  _PrintNodeList(coord_list_z_);
  printf("\n");
}

}  // namespace cross
}  // namespace aoi

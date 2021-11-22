// Copyright <disenone>

#include "squares.hpp"

#include <immintrin.h>
#include <algorithm>
#include <utility>
#include <limits>
#include <cassert>
#include <iostream>

#include <boost/timer/timer.hpp>

namespace aoi { namespace squares {


#define IfNotInXZSquare(dx, dz, x0, z0, x1, z1, radius) \
  dx = x0 - x1; \
  dz = z0 - z1; \
  if (dx > radius  || dz > radius) \

#define IfInXZRadiusSquare(dx, dz, x0, z0, x1, z1, radius_square) \
  dx = x0 - x1; \
  dz = z0 - z1; \
  if (dx * dx + dz * dz <= radius_square) \


#define IfNotInXZRadiusSquare(dx, dz, x0, z0, x1, z1, radius_square) \
  dx = x0 - x1; \
  dz = z0 - z1; \
  if (dx * dx + dz * dz > radius_square) \


BOOST_FORCEINLINE float CalcAoiDistSquare(const Pos &a, const Pos &b) {
  float dx = a.x - b.x;
  float dz = a.z - b.z;
  return dx * dx + dz * dz;
}


SquareAoi::SquareAoi(float square_size /*= 200*/)
    : square_size_(square_size),
      inverse_square_size_(1 / square_size),
      cur_aoi_map_idx_(0) {
  player_map_.reserve(100);
  squares_.reserve(100);
}


void SquareAoi::_RemoveFromSquare(Nuid nuid, PlayerAoi* pptr) {
  if (pptr->square_index < 0) {
    return;
  }
  auto square_id = pptr->square_id;
  auto square_ptr = squares_.find(square_id);
  if (square_ptr != squares_.end()) {
    auto &square = square_ptr->second;
    auto &last = square[square.size() - 1];
    auto &remove = square[pptr->square_index];
    std::swap(remove, last);
    square.pop_back();
  }
  pptr->square_index = -1;
}


void SquareAoi::_AddToSquare(Nuid nuid, PlayerAoi* pptr) {
  auto square_id = PosToId(pptr->pos.x, pptr->pos.z, inverse_square_size_);
  auto &square = squares_[square_id];
  square.push_back(pptr);
  pptr->square_id = square_id;
  pptr->square_index = square.size() - 1;
}


void SquareAoi::AddPlayer(Nuid nuid, float x, float y, float z) {
  auto piter = player_map_.find(nuid);
  PlayerAoi* pptr = nullptr;

  if (piter != player_map_.end()) {
    _RemoveFromSquare(nuid, piter->second.get());
    pptr = piter->second.get();
    pptr->UnsetFlag_Removed();
  } else {
    auto ret = player_map_.emplace(nuid, new PlayerAoi(nuid, x, y, z));
    if (ret.second) {
      pptr = ret.first->second.get();
      pptr->SetFlag_New();
    } else {
      return;
    }
  }

  _AddToSquare(nuid, pptr);
}


void SquareAoi::RemovePlayer(Nuid nuid) {
  auto piter = player_map_.find(nuid);

  if (piter != player_map_.end()) {
    auto &player = *piter->second;
    _RemoveFromSquare(nuid, &player);
    player.SetFlag_Removed();
  }
}


void SquareAoi::AddSensor(Nuid nuid, Nuid sensor_id, float radius) {
  auto piter = player_map_.find(nuid);

  if (piter == player_map_.end())
    return;

  auto& player = *piter->second;
  for (const auto& sensor : player.sensors) {
    if (sensor.sensor_id == sensor_id)
      return;
  }
  player.sensors.emplace_back(sensor_id, radius);
}


void SquareAoi::UpdatePos(Nuid nuid, float x, float y, float z) {
  auto piter = player_map_.find(nuid);

  if (piter == player_map_.end())
    return;

  auto& player = *piter->second;
  auto new_square_id = PosToId(x, z, inverse_square_size_);
  auto old_square_id = PosToId(
    player.pos.x, player.pos.z, inverse_square_size_);

  if (old_square_id != new_square_id) {
    _RemoveFromSquare(nuid, &player);
    player.pos.Set(x, y, z);
    _AddToSquare(nuid, &player);
  } else {
    player.pos.Set(x, y, z);
  }
}


AoiUpdateInfos SquareAoi::Tick() {
  // 全量做一遍 aoi
  AoiUpdateInfos update_infos;
  PlayerPtrList remove_list;

  for (auto& elem : player_map_) {
    auto& player = *elem.second;
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
    player_map_.erase(pptr->nuid);
  }
  for (auto& elem : player_map_) {
    auto& player = *elem.second;
    player.last_pos = player.pos;
  }
  cur_aoi_map_idx_ = 1 - cur_aoi_map_idx_;
  return update_infos;
}


AoiUpdateInfo SquareAoi::_UpdatePlayerAoi(Uint32 cur_aoi_map_idx,
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


void SquareAoi::_CalcAoiPlayers(const PlayerAoi& player, const Sensor& sensor,
                                PlayerPtrList* aoi_map) {
  Nuid player_nuid = player.nuid;
  float pos_x = player.pos.x;
  float pos_z = player.pos.z;
  float radius = sensor.radius;
  float radius_square = radius * radius;

  std::vector<SquarePlayers*> check_squares;
  size_t max_num = 0;
  _GetSquaresAndPlayerNum(player.pos, radius, &check_squares, &max_num);

  aoi_map->clear();
  aoi_map->reserve(max_num);
  assert(max_num <= player_map_.size());

  float dx, dz;

  for (auto square : check_squares) {
    for (auto other_ptr : *square) {
      if (other_ptr->nuid == player_nuid || other_ptr->GetFlag_Removed()) continue;
      IfNotInXZSquare(dx, dz, pos_x, pos_z, other_ptr->pos.x, other_ptr->pos.z, radius) continue;
      if (dx * dx + dz * dz < radius_square) {
        aoi_map->push_back(other_ptr);
      }
    }
  }
}


void SquareAoi::_CheckLeave(PlayerAoi* pptr, float radius_square,
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


void SquareAoi::_CheckEnter(PlayerAoi* pptr, float radius_square,
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

}  // namespace squares

}  // namespace aoi

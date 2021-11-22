// Copyright <disenone>
#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <cmath>
#include <limits>
#include <memory>

#include "common/base_types.hpp"

namespace aoi { namespace squares {

class PlayerAoi;
typedef std::unordered_map<Nuid, std::shared_ptr<PlayerAoi>> PlayerMap;
typedef std::unordered_map<Nuid, PlayerAoi*> PlayerPtrMap;
typedef std::vector<Nuid> PlayerNuids;
typedef std::vector<PlayerAoi*> PlayerPtrList;
typedef Uint64 SquareId;
typedef std::vector<PlayerAoi*> SquarePlayers;
typedef std::unordered_map<SquareId, SquarePlayers> SquareList;
constexpr int kSquareIdShift = sizeof(SquareId) * 4;

#define AOI_FLOAT_MAX std::numeric_limits<float>::max()
#define AOI_INF_POS AOI_FLOAT_MAX, AOI_FLOAT_MAX, AOI_FLOAT_MAX

struct Pos {
  Pos(float _x, float _y, float _z)
      : x(_x), y(_y), z(_z) {}

  void Set(float _x, float _y, float _z) {
    x = _x;
    y = _y;
    z = _z;
  }

  float x, y, z;
};


struct Sensor {
  Sensor(Nuid _sensor_id, float _radius)
      : sensor_id(_sensor_id), radius(_radius), radius_square(_radius * _radius) {}

  Nuid sensor_id;
  float radius;
  float radius_square;
  PlayerPtrList aoi_players[2];
};


struct PlayerAoi {
  PlayerAoi(Uint64 _nuid, float _x, float _y, float _z)
      : nuid(_nuid), pos(_x, _y, _z), last_pos(AOI_INF_POS), flags(0) {}

  AOI_CLASS_ADD_FLAG(Removed, 0, flags);
  AOI_CLASS_ADD_FLAG(New, 1, flags);

  Nuid nuid;
  SquareId square_id;
  int square_index;
  Pos pos;
  Pos last_pos;
  Uint32 flags;
  std::vector<Sensor> sensors;
};


struct SensorUpdateInfo {
  Nuid sensor_id;
  PlayerNuids enters;
  PlayerNuids leaves;
};


struct AoiUpdateInfo {
  Nuid nuid;
  std::vector<SensorUpdateInfo> sensor_update_list;
};

typedef std::unordered_map<Nuid, AoiUpdateInfo> AoiUpdateInfos;


inline int CoordToId(float coord, float inverse_square_size) {
  return static_cast<int>(std::floor(coord * inverse_square_size));
}


inline SquareId GenSquareId(int xi, int zi) {
  return (static_cast<SquareId>(xi) << kSquareIdShift) | static_cast<Uint32>(zi);
}


inline SquareId PosToId(float x, float z, float inverse_square_size) {
  int left = CoordToId(x, inverse_square_size);
  int right = CoordToId(z, inverse_square_size);
  return GenSquareId(left, right);
}


class SquareAoi {
 public:
  explicit SquareAoi(float square_size = 200);

  void AddPlayer(Nuid nuid, float x, float y, float z);
  void RemovePlayer(Nuid nuid);
  void AddSensor(Nuid nuid, Nuid sensor_id, float radius);
  void UpdatePos(Nuid nuid, float x, float y, float z);
  AoiUpdateInfos Tick();
  const SquareList& GetSquares() const {
    return squares_;
  }
  const PlayerMap& GetPlayerMap() const {
    return player_map_;
  }

 protected:
  void _AddToSquare(Nuid nuid, PlayerAoi*);
  void _RemoveFromSquare(Nuid nuid, PlayerAoi*);
  AoiUpdateInfo _UpdatePlayerAoi(Uint32 cur_aoi_map_idx, PlayerAoi* player);
  void _CalcAoiPlayers(const PlayerAoi& player, const Sensor& sensor, PlayerPtrList* aoi_map);
  inline void _GetSquaresAndPlayerNum(const Pos& pos, float radius,
                                      std::vector<SquarePlayers*> *squares, size_t* player_num);
  void _CheckLeave(PlayerAoi* pptr, float radius_square,
                    const PlayerPtrList &aoi_players, PlayerNuids *leaves);
  void _CheckEnter(PlayerAoi* pptr, float radius_square,
                    const PlayerPtrList &aoi_players, PlayerNuids *leaves);

 protected:
  float square_size_;
  float inverse_square_size_;
  Uint32 cur_aoi_map_idx_;

  SquareList squares_;
  PlayerMap player_map_;
};

inline void SquareAoi::_GetSquaresAndPlayerNum(const Pos& pos, float radius,
                                        std::vector<SquarePlayers*> *squares,
                                        size_t* player_num) {
  float pos_x = pos.x;
  float pos_z = pos.z;
  int minxi = CoordToId(pos_x - radius, inverse_square_size_);
  int maxxi = CoordToId(pos_x + radius, inverse_square_size_);
  int minzi = CoordToId(pos_z - radius, inverse_square_size_);
  int maxzi = CoordToId(pos_z + radius, inverse_square_size_);

  *player_num = 0;
  for (int xi = minxi; xi <= maxxi; ++xi) {
    for (int zi = minzi; zi <= maxzi; ++zi) {
      auto square_id = GenSquareId(xi, zi);
      auto square_iter = squares_.find(square_id);
      if (square_iter == squares_.end())
        continue;
      squares->push_back(&(square_iter->second));
      *player_num += square_iter->second.size();
    }
  }
}


}   // namespace squares
}   // namespace aoi

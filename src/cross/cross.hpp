// Copyright <disenone>

#pragma once

#include <limits>
#include <vector>
#include <memory>
#include <boost/unordered_map.hpp>

#include "common/khash.h"
#include "common/nuid.hpp"
#include "common/base_types.hpp"

namespace aoi { namespace cross {

#define COORD_TYPE_PLAYER   1
#define COORD_TYPE_GUARD_LEFT   2
#define COORD_TYPE_GUARD_RIGHT  3
#define AOI_FLOAT_LOWEST std::numeric_limits<float>::lowest()
#define AOI_INF_POS AOI_FLOAT_LOWEST, AOI_FLOAT_LOWEST, AOI_FLOAT_LOWEST

class PlayerAoi;
class Sensor;

#define AOI_HASH_MAP boost::unordered_map
typedef AOI_HASH_MAP<Nuid, std::shared_ptr<PlayerAoi>> PlayerMap;
typedef AOI_HASH_MAP<Nuid, PlayerAoi*> PlayerPtrMap;
typedef std::vector<Nuid> PlayerNuids;
typedef std::vector<PlayerAoi*> PlayerPtrList;


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


struct CoordNode {
  CoordNode(Uint8 _coord_type, float _coord_value,
            PlayerAoi *_pplayer = nullptr, Sensor *_psensor = nullptr)
    : type(_coord_type), value(_coord_value), pplayer(_pplayer), psensor(_psensor)
  {}

  Uint8 type;
  float value;
  CoordNode *prev = nullptr;
  CoordNode *next = nullptr;
  PlayerAoi *pplayer;
  Sensor *psensor;

  void PrintLog();
};


KHASH_MAP_INIT_INT64(SensorHashMap, PlayerAoi*);

struct Sensor {
  Sensor(Nuid _sensor_id, float _radius, PlayerAoi *_pplayer);
  inline void AddCandidate(PlayerAoi* other_pplayer);
  inline void RemoveCandidate(PlayerAoi* other_pplayer);
  inline void PrintCandidate();

  Nuid sensor_id;
  float radius;
  float radius_square;
  PlayerAoi *pplayer;
  CoordNode left_x;
  CoordNode right_x;
  CoordNode left_z;
  CoordNode right_z;
  PlayerPtrList aoi_players[2];

  std::shared_ptr<khash_t(SensorHashMap)> aoi_player_candidates;
};


struct PlayerAoi {
  PlayerAoi(Uint64 _nuid, float _x, float _y, float _z)
      : nuid(_nuid), pos(_x, _y, _z), last_pos(AOI_INF_POS), flags(0),
        node_x(COORD_TYPE_PLAYER, AOI_FLOAT_LOWEST, this),
        node_z(COORD_TYPE_PLAYER, AOI_FLOAT_LOWEST, this)
  {}

  AOI_CLASS_ADD_FLAG(Removed, 0, flags);
  AOI_CLASS_ADD_FLAG(Dirty, 1, flags);
  AOI_CLASS_ADD_FLAG(New, 2, flags);
  AOI_CLASS_ADD_FLAG(Beacon, 3, flags);

  Nuid nuid;
  Pos pos;
  Pos last_pos;
  Uint32 flags;
  CoordNode node_x;
  CoordNode node_z;
  std::vector<Sensor> sensors;
  std::shared_ptr<boost::unordered_map<Nuid, std::vector<Nuid>>> detected_by;
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

typedef AOI_HASH_MAP<Nuid, AoiUpdateInfo> AoiUpdateInfos;

class CrossAoi {
 public:
  CrossAoi(float map_bound_xmin, float map_bound_xmax, float map_bound_zmin,
           float map_bound_zmax, size_t beacon_x, size_t beacon_z, float beacon_radius);

  void AddPlayer(Nuid nuid, float x, float y, float z);
  void AddPlayerNoBeacon(Nuid nuid, float x, float y, float z);
  void RemovePlayer(Nuid nuid);
  void AddSensor(Nuid nuid, Nuid sensor_id, float radius);
  void AddSensorNoBeacon(Nuid nuid, Nuid sensor_id, float radius);
  void RemoveSensor(Nuid nuid, Nuid sensor_id);
  void UpdatePos(Nuid nuid, float x, float y, float z);
  AoiUpdateInfos Tick();
  const PlayerMap& GetPlayerMap() const {
    return player_map_;
  }

 protected:
  void _RemovePlayer(Nuid nuid);
  void UpdateSensorPos(const PlayerAoi &player, Sensor *sensor);
  void MovePlayerNode(CoordNode **list, CoordNode *pnode);
  AoiUpdateInfo _UpdatePlayerAoi(Uint32 cur_aoi_map_idx, PlayerAoi* player);
  void _CalcAoiPlayers(const PlayerAoi& player, const Sensor& sensor, PlayerPtrList* aoi_map);
  void _CheckLeave(PlayerAoi* pptr, float radius_square,
                    const PlayerPtrList &aoi_players, PlayerNuids *leaves);
  void _CheckEnter(PlayerAoi* pptr, float radius_square,
                    const PlayerPtrList &aoi_players, PlayerNuids *leaves);

 protected:
    CoordNode* coord_list_x_ = nullptr;
    CoordNode* coord_list_z_ = nullptr;
    PlayerMap player_map_;
    Uint32 cur_aoi_map_idx_ = 0;
    std::vector<PlayerAoi*> beacons;

 public:
  void _PrintNodeList(CoordNode *list);
  void PrintAllNodeList();
};

}   // namespace cross
}   // namespace aoi

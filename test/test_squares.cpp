// Copyright <disenone>

#include <unordered_map>
#include <iostream>
#include <vector>

#define BOOST_TEST_MODULE test_squares
#define BOOST_TEST_DYN_LINK
#include <boost/test/included/unit_test.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/timer/timer.hpp>
#include <boost/range/irange.hpp>

#include <common/nuid.hpp>
#include <common/silence_unused.hpp>
#include <squares/squares.hpp>

using namespace aoi;
using namespace aoi::squares;

BOOST_AUTO_TEST_SUITE(test_squares)

class Player;


class SquareAoiTest: public SquareAoi {
 public:
  SquareAoiTest(): SquareAoi(200) {}
friend class Player;
};


class Player {
 public:
  Player() : nuid_(GenNuid()), pos_(0, 0, 0) {}
  Player(Nuid nuid, Pos pos) : nuid_(nuid), pos_(pos) {}

 public:
  inline Nuid AddSensor(float radius, bool log = false) {
    auto sensor_id = GenNuid();
    aoi_->AddSensor(nuid_, sensor_id, radius);
    if (log) {
      printf("Player %lu Add Sensor %lu, radius %f\n", nuid_, sensor_id, radius);
    }
    return sensor_id;
  }

  inline void AddToAoi(SquareAoiTest* aoi, bool log = false) {
    aoi_ = aoi;
    aoi_->AddPlayer(nuid_, pos_.x, pos_.y, pos_.z);
    player_aoi_ = aoi_->player_map_.find(nuid_)->second.get();

    if (log) {
      printf("Add Player: %lu, Pos(%f, %f, %f), square_id(%lu)\n",
            nuid_, pos_.x, pos_.y, pos_.z, player_aoi_->square_id);
    }
  }

  inline void RemoveFromAoi(bool log = false) {
    if (log) {
      printf("Remove Player: %lu, Pos(%f, %f, %f), square_id(%lu)\n",
            nuid_, pos_.x, pos_.y, pos_.z, player_aoi_->square_id);
    }
    aoi_->RemovePlayer(nuid_);
    aoi_ = nullptr;
    player_aoi_ = nullptr;
  }

  inline void MoveDelta(float x, float y, float z) {
    MoveTo(pos_.x + x, pos_.y + y, pos_.z + z);
  }

  inline void MoveTo(float x, float y, float z, bool log = false) {
    pos_.Set(x, y, z);
    SquareId old_square_id = -1, new_square_id = -1;
    if (aoi_) {
      old_square_id = player_aoi_->square_id;
      aoi_->UpdatePos(nuid_, x, y, z);
      new_square_id = player_aoi_->square_id;
    }

    if (log) {
      printf("Player %lu MoveTo (%f, %f, %f), square_id (%lu -> %lu)\n",
            nuid_, x, y, z, old_square_id, new_square_id);
    }

    // BOOST_TEST(new_square_id == PosToId(x, z, aoi_->inverse_square_size_));
  }

 public:
  Nuid nuid_;
  Pos pos_;

  PlayerAoi *player_aoi_;
  SquareAoiTest *aoi_;
};


void PrintAoiUpdateInfos(const AoiUpdateInfos &info) {
  printf("=========================== update_infos\n");
  for (auto &elem : info) {
    auto& update_info = elem.second;
    printf("Player: %lu; update sensors size: %lu\n",
           update_info.nuid, update_info.sensor_update_list.size());
    for (auto &sensor : update_info.sensor_update_list) {
      printf("  Sensor_id: %lu, \n", sensor.sensor_id);

      printf("    enters: (%lu)[", sensor.enters.size());
      for (auto &nuid : sensor.enters) {
        printf("%lu, ", nuid);
      }
      printf("]\n");

      printf("    leaves: (%lu)[", sensor.leaves.size());
      for (auto &nuid : sensor.leaves) {
        printf("%lu,", nuid);
      }
      printf("]\n");
    }
  }
  printf("===========================\n\n");
}


void CheckUpdateInfos(const AoiUpdateInfos &update_infos, const AoiUpdateInfos &require_infos) {
  // BOOST_TEST_REQUIRE((update_infos == require_infos));
  BOOST_TEST_REQUIRE((update_infos.size() == require_infos.size()));
  for (const auto &pair : update_infos) {
    auto &update_info = pair.second;
    BOOST_TEST_REQUIRE((require_infos.find(update_info.nuid) != require_infos.end()));
    auto &require_info = require_infos.find(update_info.nuid)->second;

    BOOST_TEST_REQUIRE(
      (update_info.sensor_update_list.size() == require_info.sensor_update_list.size()));

    for (size_t i =0; i < update_info.sensor_update_list.size(); ++i) {
      auto &sensor_info = update_info.sensor_update_list[i];
      auto &require_sensor_info = require_info.sensor_update_list[i];
      BOOST_TEST_REQUIRE((sensor_info.sensor_id == require_sensor_info.sensor_id));
      BOOST_TEST_REQUIRE((sensor_info.enters == require_sensor_info.enters));
      BOOST_TEST_REQUIRE((sensor_info.leaves == require_sensor_info.leaves));
    }
  }
}


BOOST_AUTO_TEST_CASE(test_suqare_id) {
  Pos pos(0, 0, 0);
  float inverse_square_size = 1/200.;
  BOOST_TEST_REQUIRE((PosToId(pos.x, pos.z, inverse_square_size) == 0UL));
  pos.Set(-1, 0, 0);
  BOOST_TEST_REQUIRE((PosToId(pos.x, pos.z, inverse_square_size) == 18446744069414584320UL));
  pos.Set(0, 0, -1);
  BOOST_TEST_REQUIRE((PosToId(pos.x, pos.z, inverse_square_size) == 4294967295UL));
  pos.Set(-1, 0, -1);
  BOOST_TEST_REQUIRE((PosToId(pos.x, pos.z, inverse_square_size) == 18446744073709551615UL));
}


void TestSimple(bool log = false) {
  SquareAoiTest square_aoi;

  Player player1{GenNuid(), {0, 0, 0}};
  player1.AddToAoi(&square_aoi, log);
  auto sensor_id1 = player1.AddSensor(10, log);

  Player player2{GenNuid(), {0, 0, 0}};
  player2.AddToAoi(&square_aoi, log);
  auto sensor_id2 = player2.AddSensor(5, log);

  auto update_infos = square_aoi.Tick();
  if (log)
    PrintAoiUpdateInfos(update_infos);

  AoiUpdateInfos require_infos = {
      {player1.nuid_, {player1.nuid_, {{sensor_id1, {player2.nuid_}, {}}}}},
      {player2.nuid_, {player2.nuid_, {{sensor_id2, {player1.nuid_}, {}}}}},
  };
  CheckUpdateInfos(update_infos, require_infos);

  // player2 move to (6, 0, 0)
  player2.MoveTo(6, 0, 0, log);

  update_infos = square_aoi.Tick();
  if (log)
    PrintAoiUpdateInfos(update_infos);

  require_infos = {
      {player2.nuid_, {player2.nuid_, {{sensor_id2, {}, {player1.nuid_}}}}},
    };
  CheckUpdateInfos(update_infos, require_infos);

  // player2 move to (600, 0, 100)
  player2.MoveTo(600, 0, 100, log);
  update_infos = square_aoi.Tick();
  if (log)
    PrintAoiUpdateInfos(update_infos);
  require_infos = {
      {player1.nuid_, {player1.nuid_, {{sensor_id1, {}, {player2.nuid_}}}}},
    };
  CheckUpdateInfos(update_infos, require_infos);

  const auto &squares = square_aoi.GetSquares();
  BOOST_TEST_REQUIRE((squares.size() == 2));
  for (const auto &elem : squares) {
    BOOST_TEST_REQUIRE((elem.second.size() == 1));
  }

  player1.MoveTo(601, 100, 101, log);
  update_infos = square_aoi.Tick();
  if (log)
    PrintAoiUpdateInfos(update_infos);
  require_infos = {
      {player1.nuid_, {player1.nuid_, {{sensor_id1, {player2.nuid_}, {}}}}},
      {player2.nuid_, {player2.nuid_, {{sensor_id2, {player1.nuid_}, {}}}}},
  };
  CheckUpdateInfos(update_infos, require_infos);

  player2.RemoveFromAoi(log);
  update_infos = square_aoi.Tick();
  if (log)
    PrintAoiUpdateInfos(update_infos);
  require_infos = {
      {player1.nuid_, {player1.nuid_, {{sensor_id1, {}, {player2.nuid_}}}}},
  };
  CheckUpdateInfos(update_infos, require_infos);
}


BOOST_AUTO_TEST_CASE(test_simple) {
  bool log = false;
  TestSimple(log);
}


std::vector<Player> GenPlayers(const size_t player_num, const float map_size) {
  std::vector<Player> players(player_num);

  boost::random::mt19937 random_generator(std::time(0));
  boost::random::uniform_real_distribution<float> pos_generator(-map_size, map_size);

  for (int i : boost::irange(player_num)) {
    auto &player = players[i];
    player.pos_.Set(pos_generator(random_generator), 0, pos_generator(random_generator));
  }

  BOOST_TEST_REQUIRE((players.size() == player_num));
  return players;
}

std::vector<Pos> GenMovements(const size_t player_num, const float length) {
  std::vector<Pos> movements;
  movements.reserve(player_num);

  boost::random::mt19937 random_generator(std::time(0));
  boost::random::uniform_real_distribution<float> angle_gen(0, 360);
  for (int UNUSED(i) : boost::irange(player_num)) {
    float angle = angle_gen(random_generator);
    float radian = 2 * M_PI * angle / 360;
    movements.emplace_back(std::cos(radian) * length, 0, std::sin(radian) * length);
  }

  return movements;
}

void TestOneMilestone(std::vector<Player> *players, const size_t player_num, const float map_size) {
  printf("\n===Begin Milestore: player_num = %lu, map_size = (%f, %f)\n",
         player_num, -map_size, map_size);

  boost::timer::cpu_timer run_timer;
  int times = 1;
  std::vector<SquareAoiTest> square_aois(times);
  for (auto &square_aoi : square_aois) {
    for (auto &player : *players) {
      player.AddToAoi(&square_aoi);
      player.AddSensor(100);
    }
    BOOST_TEST_REQUIRE((square_aoi.GetPlayerMap().size() == player_num));
  }
  run_timer.stop();
  printf("Add Player (%i times)", times);
  std::cout << run_timer.format();

  for (auto &square_aoi : square_aois) {
    square_aoi.Tick();
  }

  run_timer.start();
  for (auto &square_aoi : square_aois) {
    square_aoi.Tick();
  }
  run_timer.stop();

  printf("Tick (%i times)", times);
  std::cout << run_timer.format();

  float speed = 6;
  float delta_time = 0.1;
  auto movements = GenMovements(player_num, delta_time * speed);
  times = 1 / delta_time;
  run_timer.start();
  for (int UNUSED(t) : boost::irange(times)) {
    for (int i : boost::irange(player_num)) {
      auto &player = players->at(i);
      auto &move = movements[i];
      player.MoveDelta(move.x, move.y, move.z);
    }
  }
  run_timer.stop();
  printf("Update Pos (%i times)", times);
  std::cout << run_timer.format();

  printf("===End Milestore\n");
}


BOOST_AUTO_TEST_CASE(test_milestone) {
  for (size_t player_num : {100, 1000, 10000}) {
    for (float map_size : {50, 100, 1000, 10000}) {
      auto players = GenPlayers(player_num, map_size);
      TestOneMilestone(&players, player_num, map_size);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

// Copyright <disenone>

#include "nuid.hpp"

#include <ctime>
#include <random>

namespace aoi {

Nuid NuidGenerator::nuid_ = NuidGenerator::InitNuid();

Nuid NuidGenerator::InitNuid() {
  srand((unsigned)std::time(NULL));
  return rand();
}

Nuid GenNuid() {
  return NuidGenerator::nuid_++;
}

}  // namespace aoi

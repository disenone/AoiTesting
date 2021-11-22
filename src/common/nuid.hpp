// Copyright <disenone>

#pragma once

#include "common/base_types.hpp"

namespace aoi {

class NuidGenerator {
 private:
  static Nuid nuid_;

  static Nuid InitNuid();

  friend Nuid GenNuid();
};

Nuid GenNuid();

}

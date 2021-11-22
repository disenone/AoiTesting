// Copyright <disenone>
#pragma once

#include <stdint.h>

namespace aoi {

typedef uint8_t  Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef Uint64 Nuid;

typedef int16_t Int16;

#define AOI_CLASS_ADD_FLAG(flag_name, flag_idx, flags_name)   \
  inline void SetFlag_##flag_name() {flags_name |= 1 << flag_idx;}   \
  inline void UnsetFlag_##flag_name() {flags_name &= ~(1 << flag_idx);}    \
  inline bool GetFlag_##flag_name() const {return flags & 1 << flag_idx;}

}

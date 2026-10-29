// Relógio e cores solares do Horizonte.
#pragma once
#include <algorithm>
#include <cmath>
#include "theme.h"
namespace zz::horizonte {
// elapsed seconds is persisted by SaveCodec; one day is exactly 24,000 ticks.
inline double dayTick(double seconds) {
  double tick = std::fmod(std::floor(std::max(0.0, seconds) * 20.0), 24000.0);
  return tick;
}
inline float daylight(double seconds) {
  const double tick = dayTick(seconds);
  if (tick < 12000)
    return 1.f;
  if (tick < 14000)
    return float((14000 - tick) / 2000);
  if (tick < 22000)
    return 0.f;
  return float((tick - 22000) / 2000);
}
inline bool villagerWorking(double seconds) {
  double tick = dayTick(seconds);
  return tick >= 2000 && tick < 9000;
}
} // namespace hz

namespace zz::horizonte {
inline SkyPreset cycleSky(SkyPreset sky, double seconds) {
    float day = daylight(seconds);
    sky.zenith = lerp(Vec3(.012f,.018f,.055f), Vec3(.30f,.55f,.91f), day);
    sky.horizon = lerp(Vec3(.025f,.035f,.075f), Vec3(.68f,.81f,.98f), day);
    sky.fog = sky.horizon;
    sky.ambient = lerp(Vec3(.07f,.08f,.13f), Vec3(.48f,.48f,.48f), day);
    sky.sunIntensity = .06f + .94f * day;
    sky.night = 1 - day;
    float angle = float(dayTick(seconds) / 24000.0) * kTwoPi;
    sky.sunDirection = normalize(Vec3(std::cos(angle),std::sin(angle),-.25f));
    return sky;
}
}

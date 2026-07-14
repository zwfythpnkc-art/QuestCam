#pragma once

#include <cmath>

namespace QuestCam {

struct Vec3 {
  float x{};
  float y{};
  float z{};
};

struct Quaternion {
  float x{};
  float y{};
  float z{};
  float w{1.0f};
};

struct Pose {
  Vec3 position{};
  Quaternion rotation{};
};

struct SmoothingSettings {
  // A half-life of zero disables smoothing on that channel.
  float positionHalfLifeSeconds{0.08f};
  float rotationHalfLifeSeconds{0.12f};
};

class PoseSmoother {
 public:
  explicit PoseSmoother(SmoothingSettings settings = {});

  void SetSettings(SmoothingSettings settings);
  void Reset(Pose pose);
  [[nodiscard]] Pose Update(Pose target, float deltaTimeSeconds);
  [[nodiscard]] bool IsInitialized() const { return initialized_; }

 private:
  SmoothingSettings settings_;
  Pose current_{};
  bool initialized_{false};
};

[[nodiscard]] float HalfLifeAlpha(float halfLifeSeconds, float deltaTimeSeconds);
[[nodiscard]] Vec3 Lerp(Vec3 from, Vec3 to, float alpha);
[[nodiscard]] Quaternion Normalize(Quaternion value);
[[nodiscard]] Quaternion Slerp(Quaternion from, Quaternion to, float alpha);

}  // namespace QuestCam

#include "questcam/PoseSmoother.hpp"

#include <algorithm>

namespace QuestCam {
namespace {
constexpr float kLn2 = 0.6931471805599453f;
constexpr float kEpsilon = 1.0e-6f;
}

float HalfLifeAlpha(float halfLifeSeconds, float deltaTimeSeconds) {
  if (deltaTimeSeconds <= 0.0f) return 0.0f;
  if (halfLifeSeconds <= 0.0f) return 1.0f;
  return 1.0f - std::exp(-kLn2 * deltaTimeSeconds / halfLifeSeconds);
}

Vec3 Lerp(Vec3 from, Vec3 to, float alpha) {
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  return {
      from.x + (to.x - from.x) * alpha,
      from.y + (to.y - from.y) * alpha,
      from.z + (to.z - from.z) * alpha,
  };
}

Quaternion Normalize(Quaternion value) {
  const float magnitude = std::sqrt(value.x * value.x + value.y * value.y +
                                    value.z * value.z + value.w * value.w);
  if (magnitude <= kEpsilon) return {};
  return {value.x / magnitude, value.y / magnitude, value.z / magnitude,
          value.w / magnitude};
}

Quaternion Slerp(Quaternion from, Quaternion to, float alpha) {
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  from = Normalize(from);
  to = Normalize(to);

  float dot = from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;
  if (dot < 0.0f) {
    to = {-to.x, -to.y, -to.z, -to.w};
    dot = -dot;
  }

  if (dot > 0.9995f) {
    return Normalize({
        from.x + alpha * (to.x - from.x),
        from.y + alpha * (to.y - from.y),
        from.z + alpha * (to.z - from.z),
        from.w + alpha * (to.w - from.w),
    });
  }

  dot = std::clamp(dot, -1.0f, 1.0f);
  const float theta = std::acos(dot);
  const float sinTheta = std::sin(theta);
  const float fromWeight = std::sin((1.0f - alpha) * theta) / sinTheta;
  const float toWeight = std::sin(alpha * theta) / sinTheta;
  return {
      from.x * fromWeight + to.x * toWeight,
      from.y * fromWeight + to.y * toWeight,
      from.z * fromWeight + to.z * toWeight,
      from.w * fromWeight + to.w * toWeight,
  };
}

PoseSmoother::PoseSmoother(SmoothingSettings settings) : settings_(settings) {}

void PoseSmoother::SetSettings(SmoothingSettings settings) {
  settings.positionHalfLifeSeconds =
      std::max(0.0f, settings.positionHalfLifeSeconds);
  settings.rotationHalfLifeSeconds =
      std::max(0.0f, settings.rotationHalfLifeSeconds);
  settings_ = settings;
}

void PoseSmoother::Reset(Pose pose) {
  current_ = pose;
  current_.rotation = Normalize(current_.rotation);
  initialized_ = true;
}

Pose PoseSmoother::Update(Pose target, float deltaTimeSeconds) {
  target.rotation = Normalize(target.rotation);
  if (!initialized_ || deltaTimeSeconds > 0.5f) {
    Reset(target);
    return current_;
  }

  current_.position = Lerp(
      current_.position, target.position,
      HalfLifeAlpha(settings_.positionHalfLifeSeconds, deltaTimeSeconds));
  current_.rotation = Slerp(
      current_.rotation, target.rotation,
      HalfLifeAlpha(settings_.rotationHalfLifeSeconds, deltaTimeSeconds));
  return current_;
}

}  // namespace QuestCam

#include "questcam/PoseSmoother.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {
bool Near(float left, float right, float epsilon = 1.0e-4f) {
  return std::abs(left - right) <= epsilon;
}
}

int main() {
  using namespace QuestCam;

  assert(Near(HalfLifeAlpha(0.1f, 0.1f), 0.5f));
  assert(Near(HalfLifeAlpha(0.0f, 0.1f), 1.0f));
  assert(Near(HalfLifeAlpha(0.1f, 0.0f), 0.0f));

  PoseSmoother smoother({0.1f, 0.1f});
  smoother.Reset({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}});
  const Pose halfway = smoother.Update(
      {{10.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}}, 0.1f);
  assert(Near(halfway.position.x, 5.0f));
  assert(Near(halfway.rotation.y, 0.7071067f));
  assert(Near(halfway.rotation.w, 0.7071067f));

  const Pose afterPause = smoother.Update(
      {{3.0f, 4.0f, 5.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, 0.75f);
  assert(Near(afterPause.position.x, 3.0f));
  assert(Near(afterPause.position.y, 4.0f));
  assert(Near(afterPause.position.z, 5.0f));

  std::cout << "PoseSmoother tests passed\n";
}

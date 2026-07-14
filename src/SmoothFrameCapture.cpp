#include "questcam/SmoothFrameCapture.hpp"

#include "questcam/FrameServer.hpp"

#include "UnityEngine/ImageConversion.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"

#include <algorithm>
#include <vector>

DEFINE_TYPE(QuestCam, SmoothFrameCapture);

namespace QuestCam {

void SmoothFrameCapture::ctor() { INVOKE_CTOR(); }

void SmoothFrameCapture::Configure(UnityEngine::RenderTexture* texture,
                                   int captureWidth, int captureHeight,
                                   int framesPerSecond, int jpegQuality) {
  target = texture;
  width = std::clamp(captureWidth, 640, 1920);
  height = std::clamp(captureHeight, 360, 1080);
  fps = std::clamp(framesPerSecond, 10, 60);
  quality = std::clamp(jpegQuality, 35, 85);
  if (readableTexture) UnityEngine::Object::Destroy(readableTexture);
  readableTexture = UnityEngine::Texture2D::New_ctor(width, height);
  nextFrameTime = 0.0f;
}

void SmoothFrameCapture::Capture(UnityEngine::Camera* camera,
                                 UnityEngine::Vector3 position,
                                 UnityEngine::Quaternion rotation,
                                 float fieldOfView) {
  // Render at the configured low frame rate even before the browser finishes
  // opening its MJPEG request. Some Quest WebViews never exposed that client
  // state to the Unity thread, leaving an otherwise working camera black.
  if (!camera || !target || !readableTexture ||
      !FrameServer::Instance().IsRunning()) return;
  const float now = UnityEngine::Time::get_unscaledTime();
  if (now < nextFrameTime) return;
  nextFrameTime = now + 1.0f / static_cast<float>(fps);

  // Render through Beat Saber's real camera instead of a bare Camera copy.
  // Camera-attached effects (including Vivify) are otherwise absent, which is
  // what caused the flat grey world and black interaction rectangles. Restore
  // every headset-camera property immediately after this off-screen render.
  auto transform = camera->get_transform();
  const auto headsetPosition = transform->get_position();
  const auto headsetRotation = transform->get_rotation();
  auto headsetTarget = camera->get_targetTexture();
  const float headsetFov = camera->get_fieldOfView();
  const float headsetAspect = camera->get_aspect();
  const auto headsetStereoEye = camera->get_stereoTargetEye();

  transform->set_position(position);
  transform->set_rotation(rotation);
  camera->set_fieldOfView(fieldOfView);
  camera->set_aspect(static_cast<float>(width) / static_cast<float>(height));
  camera->set_targetTexture(target);
  camera->set_stereoTargetEye(UnityEngine::StereoTargetEyeMask::None);
  camera->Render();
  camera->set_targetTexture(headsetTarget);
  camera->set_stereoTargetEye(headsetStereoEye);
  camera->set_fieldOfView(headsetFov);
  camera->set_aspect(headsetAspect);
  transform->set_position(headsetPosition);
  transform->set_rotation(headsetRotation);
  auto previous = UnityEngine::RenderTexture::get_active();
  UnityEngine::RenderTexture::set_active(target);
  readableTexture->ReadPixels(UnityEngine::Rect(0, 0, width, height), 0, 0,
                              false);
  readableTexture->Apply(false, false);
  UnityEngine::RenderTexture::set_active(previous);

  const auto encoded =
      UnityEngine::ImageConversion::EncodeToJPG(readableTexture, quality);
  if (!encoded || encoded.size() == 0) return;
  FrameServer::Instance().PublishJpeg(
      std::vector<uint8_t>(encoded.begin(), encoded.end()));
}

void SmoothFrameCapture::OnDestroy() {
  if (readableTexture) UnityEngine::Object::Destroy(readableTexture);
  readableTexture = nullptr;
  target = nullptr;
}

}  // namespace QuestCam

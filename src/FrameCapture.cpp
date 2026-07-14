#include "questcam/FrameCapture.hpp"

#include "questcam/FrameServer.hpp"

#include "UnityEngine/ImageConversion.hpp"
#include "UnityEngine/Graphics.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/Time.hpp"

#include <algorithm>
#include <vector>

DEFINE_TYPE(QuestCam, FrameCapture);

namespace QuestCam {

void FrameCapture::ctor() { INVOKE_CTOR(); }

void FrameCapture::Configure(int captureWidth, int captureHeight,
                             int framesPerSecond, int jpegQuality) {
  width = std::clamp(captureWidth, 640, 1920);
  height = std::clamp(captureHeight, 360, 1080);
  fps = std::clamp(framesPerSecond, 10, 60);
  quality = std::clamp(jpegQuality, 35, 95);
  if (target) UnityEngine::Object::Destroy(target);
  if (readableTexture) UnityEngine::Object::Destroy(readableTexture);
  target = UnityEngine::RenderTexture::New_ctor(width, height, 0);
  target->set_name("QuestCam Final Frame");
  readableTexture = UnityEngine::Texture2D::New_ctor(width, height);
}

void FrameCapture::OnRenderImage(UnityEngine::RenderTexture* source,
                                 UnityEngine::RenderTexture* destination) {
  // Preserve Beat Saber's normal XR render chain first. This captures the
  // already-rendered eye image, so Vivify and other map effects stay intact.
  if (source) UnityEngine::Graphics::Blit(source, destination);
  if (!source || !target || !readableTexture ||
      !FrameServer::Instance().IsRunning() ||
      !FrameServer::Instance().HasVideoClients()) return;

  const float now = UnityEngine::Time::get_unscaledTime();
  if (now < nextFrameTime) return;
  nextFrameTime = now + 1.0f / static_cast<float>(fps);

  // One GPU downscale replaces the old, expensive second world render.
  UnityEngine::Graphics::Blit(source, target);

  auto previous = UnityEngine::RenderTexture::get_active();
  UnityEngine::RenderTexture::set_active(target);
  readableTexture->ReadPixels(UnityEngine::Rect(0, 0, width, height), 0, 0, false);
  readableTexture->Apply(false, false);
  UnityEngine::RenderTexture::set_active(previous);

  const auto encoded = UnityEngine::ImageConversion::EncodeToJPG(readableTexture, quality);
  if (!encoded || encoded.size() == 0) return;
  FrameServer::Instance().PublishJpeg(
      std::vector<uint8_t>(encoded.begin(), encoded.end()));
}

void FrameCapture::OnDestroy() {
  if (target) UnityEngine::Object::Destroy(target);
  if (readableTexture) UnityEngine::Object::Destroy(readableTexture);
  readableTexture = nullptr;
  target = nullptr;
}

}  // namespace QuestCam

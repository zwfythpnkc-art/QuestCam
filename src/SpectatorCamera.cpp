#include "questcam/SpectatorCamera.hpp"

#include "main.hpp"
#include "questcam/AudioCapture.hpp"
#include "questcam/FrameCapture.hpp"
#include "questcam/FrameServer.hpp"
#include "questcam/SmoothFrameCapture.hpp"

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/StereoTargetEyeMask.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector3.hpp"

#include <algorithm>

DEFINE_TYPE(QuestCam, SpectatorCamera);

namespace QuestCam {

bool SpectatorCamera::UseSmoothMode() const {
  return wideSmoothMode || automaticSmoothFallback;
}

void SpectatorCamera::ctor() {
  INVOKE_CTOR();
  ReloadSettings();
}

void SpectatorCamera::ReloadSettings() {
  auto& json = getConfig().config;
  const auto readBool = [&json](const char* key, bool fallback) {
    return json.IsObject() && json.HasMember(key) && json[key].IsBool()
               ? json[key].GetBool()
               : fallback;
  };
  const auto readFloat = [&json](const char* key, float fallback) {
    return json.IsObject() && json.HasMember(key) && json[key].IsNumber()
               ? json[key].GetFloat()
               : fallback;
  };
  const auto readInt = [&json](const char* key, int fallback) {
    return json.IsObject() && json.HasMember(key) && json[key].IsInt()
               ? json[key].GetInt()
               : fallback;
  };

  const bool newEnabled = readBool("enabled", true);
  const bool newStreamAudio = readBool("streamAudio", true);
  const bool newWideSmoothMode = readBool("wideSmoothMode", true);
  const float newFieldOfView =
      std::clamp(readFloat("fieldOfView", 90.0f), 50.0f, 170.0f);
  const float newPositionHalfLife =
      std::clamp(readFloat("positionHalfLife", 0.08f), 0.0f, 0.5f);
  const float newRotationHalfLife =
      std::clamp(readFloat("rotationHalfLife", 0.12f), 0.0f, 0.5f);
  const int newWidth = std::clamp(readInt("width", 960), 640, 1920);
  const int newHeight = std::clamp(readInt("height", 540), 360, 1080);
  const int newFps = std::clamp(readInt("fps", 15), 10, 60);
  const int newJpegQuality =
      std::clamp(readInt("jpegQuality", 60), 35, 85);

  const bool modeChanged = newWideSmoothMode != wideSmoothMode;
  const bool dimensionsChanged = newWidth != width || newHeight != height;
  const bool encodingChanged = newFps != fps || newJpegQuality != jpegQuality;

  enabled = newEnabled;
  streamAudio = newStreamAudio;
  wideSmoothMode = newWideSmoothMode;
  fieldOfView = newFieldOfView;
  positionHalfLife = newPositionHalfLife;
  rotationHalfLife = newRotationHalfLife;
  width = newWidth;
  height = newHeight;
  fps = newFps;
  jpegQuality = newJpegQuality;
  serverPort = std::clamp(readInt("serverPort", 5353), 1024, 65535);
  smoother.SetSettings({positionHalfLife, rotationHalfLife});

  if (!enabled || modeChanged || dimensionsChanged) {
    if (sourceCamera) DestroyCamera();
    return;
  }
  if (encodingChanged) {
    if (frameCapture) frameCapture->Configure(width, height, fps, jpegQuality);
    if (smoothCapture && outputTexture) {
      smoothCapture->Configure(outputTexture, width, height, fps, jpegQuality);
    }
  }
  if (!streamAudio && audioCapture) {
    UnityEngine::Object::Destroy(audioCapture);
    audioCapture = nullptr;
  }
}

void SpectatorCamera::CreateSmoothCamera() {
  if (!UseSmoothMode() || !sourceCamera || captureWorker) return;
  captureWorker = UnityEngine::GameObject::New_ctor("QuestCam Capture Worker");
  UnityEngine::Object::DontDestroyOnLoad(captureWorker);
  outputTexture = UnityEngine::RenderTexture::New_ctor(width, height, 16);
  outputTexture->set_name("QuestCam Wide Smooth Output");
  smoothCapture = captureWorker->AddComponent<QuestCam::SmoothFrameCapture*>();
  smoothCapture->Configure(outputTexture, width, height, fps, jpegQuality);

  auto transform = sourceCamera->get_transform();
  const auto position = transform->get_position();
  const auto rotation = transform->get_rotation();
  smoother.Reset({{position.x, position.y, position.z},
                  {rotation.x, rotation.y, rotation.z, rotation.w}});
  ResetFrameWatchdog();
  PaperLogger.info("QuestCam created effects-compatible wide + smooth capture");
}

void SpectatorCamera::ResetFrameWatchdog() {
  lastPublishedFrames = FrameServer::Instance().PublishedFrames();
  lastFrameProgressAt = UnityEngine::Time::get_unscaledTime();
  nextRecoveryAt = lastFrameProgressAt + 3.0f;
}

void SpectatorCamera::RefreshSourceCamera() {
  auto activeCamera = UnityEngine::Camera::get_main();
  if (!activeCamera) {
    // During scene teardown Camera.main is briefly null. Release every object
    // that carries render state from the departing level and recreate it when
    // the next menu/gameplay camera appears.
    if (sourceCamera || captureWorker || frameCapture) DestroyCamera();
    return;
  }
  if (sourceCamera == activeCamera) return;

  // Never CopyFrom across Beat Saber scenes. Cameras retain scene-specific
  // render state and command buffers, which froze the MJPEG stream after a
  // level ended or failed. A fresh output camera is safer and deterministic.
  if (sourceCamera || captureWorker || frameCapture) DestroyCamera();
  sourceCamera = activeCamera;

  if (UseSmoothMode()) {
    CreateSmoothCamera();
  } else {
    frameCapture =
        sourceCamera->get_gameObject()->AddComponent<QuestCam::FrameCapture*>();
    frameCapture->Configure(width, height, fps, jpegQuality);
  }
  PaperLogger.info("QuestCam rebuilt capture for the active scene camera");
}

void SpectatorCamera::EnsureCamera() {
  if (!enabled) return;
  RefreshSourceCamera();
  if (!sourceCamera) return;
  if (UseSmoothMode()) {
    CreateSmoothCamera();
  } else if (!frameCapture) {
    frameCapture =
        sourceCamera->get_gameObject()->AddComponent<QuestCam::FrameCapture*>();
    frameCapture->Configure(width, height, fps, jpegQuality);
  }
  if (streamAudio && !audioCapture) {
    audioCapture =
        sourceCamera->get_gameObject()->AddComponent<QuestCam::AudioCapture*>();
  }
}

void SpectatorCamera::LateUpdate() {
  ReloadSettings();
  if (!enabled) return;
  if (!FrameServer::Instance().IsRunning()) {
    FrameServer::Instance().Start(static_cast<uint16_t>(serverPort), 48000, 2);
  }

  const int sceneHandle =
      UnityEngine::SceneManagement::SceneManager::GetActiveScene().get_handle();
  if (sceneHandle != activeSceneHandle) {
    activeSceneHandle = sceneHandle;
    if (sourceCamera || captureWorker || frameCapture) {
      PaperLogger.info("Beat Saber scene changed; rebuilding QuestCam capture");
      DestroyCamera();
    }
    ResetFrameWatchdog();
  }
  EnsureCamera();

  auto& server = FrameServer::Instance();
  if (!UseSmoothMode() && server.HasVideoClients()) {
    const float now = UnityEngine::Time::get_unscaledTime();
    if (clientWaitStartedAt <= 0.0f) {
      clientWaitStartedAt = now;
      framesWhenClientStarted = server.PublishedFrames();
    } else if (now - clientWaitStartedAt >= 2.0f &&
               server.PublishedFrames() == framesWhenClientStarted) {
      PaperLogger.warn(
          "Final-frame callback produced no video; switching to Wide + Smooth fallback");
      automaticSmoothFallback = true;
      DestroyCamera();
      EnsureCamera();
    }
  } else if (!server.HasVideoClients()) {
    clientWaitStartedAt = 0.0f;
    framesWhenClientStarted = server.PublishedFrames();
  }


  // Camera.main can survive a Beat Saber transition with the same pointer,
  // so pointer checks alone cannot detect a dead render path. If OBS is still
  // connected but no new JPEG has been published, rebuild all capture state.
  const float watchdogNow = UnityEngine::Time::get_unscaledTime();
  const uint64_t published = server.PublishedFrames();
  if (!server.HasVideoClients()) {
    lastPublishedFrames = published;
    lastFrameProgressAt = watchdogNow;
  } else if (published != lastPublishedFrames) {
    lastPublishedFrames = published;
    lastFrameProgressAt = watchdogNow;
  } else if (watchdogNow >= nextRecoveryAt &&
             watchdogNow - lastFrameProgressAt >= 2.5f) {
    PaperLogger.warn("Video stopped advancing; rebuilding capture automatically");
    DestroyCamera();
    EnsureCamera();
    ResetFrameWatchdog();
  }

  if (!UseSmoothMode() || !sourceCamera || !outputTexture || !smoothCapture)
    return;

  auto sourceTransform = sourceCamera->get_transform();
  const auto position = sourceTransform->get_position();
  const auto rotation = sourceTransform->get_rotation();
  const Pose smooth = smoother.Update(
      {{position.x, position.y, position.z},
       {rotation.x, rotation.y, rotation.z, rotation.w}},
      UnityEngine::Time::get_unscaledDeltaTime());
  smoothCapture->Capture(
      sourceCamera, {smooth.position.x, smooth.position.y, smooth.position.z},
      {smooth.rotation.x, smooth.rotation.y, smooth.rotation.z,
       smooth.rotation.w},
      fieldOfView);
}

void SpectatorCamera::DestroyCamera() {
  if (frameCapture) UnityEngine::Object::Destroy(frameCapture);
  if (audioCapture) UnityEngine::Object::Destroy(audioCapture);
  if (outputTexture) UnityEngine::Object::Destroy(outputTexture);
  if (captureWorker) UnityEngine::Object::Destroy(captureWorker);
  frameCapture = nullptr;
  audioCapture = nullptr;
  smoothCapture = nullptr;
  outputTexture = nullptr;
  captureWorker = nullptr;
  sourceCamera = nullptr;
}

void SpectatorCamera::OnDestroy() { DestroyCamera(); }

}  // namespace QuestCam

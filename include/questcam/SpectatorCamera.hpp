#pragma once

#include "custom-types/shared/macros.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/RenderTexture.hpp"

#include "questcam/PoseSmoother.hpp"

namespace QuestCam {
class FrameCapture;
class AudioCapture;
class SmoothFrameCapture;
}

DECLARE_CLASS_CODEGEN(QuestCam, SpectatorCamera, UnityEngine::MonoBehaviour) {
  DECLARE_INSTANCE_METHOD(void, LateUpdate);
  DECLARE_INSTANCE_METHOD(void, OnDestroy);
  DECLARE_CTOR(ctor);

  private:
    void EnsureCamera();
    void RefreshSourceCamera();
    void ReloadSettings();
    void DestroyCamera();
    void CreateSmoothCamera();
    void ResetFrameWatchdog();
    [[nodiscard]] bool UseSmoothMode() const;

    UnityW<UnityEngine::Camera> sourceCamera;
    UnityW<QuestCam::FrameCapture> frameCapture;
    UnityW<QuestCam::AudioCapture> audioCapture;
    UnityW<UnityEngine::GameObject> captureWorker;
    UnityW<UnityEngine::RenderTexture> outputTexture;
    UnityW<QuestCam::SmoothFrameCapture> smoothCapture;
    PoseSmoother smoother;
    bool enabled{true};
    bool streamAudio{true};
    bool wideSmoothMode{true};
    bool automaticSmoothFallback{false};
    float clientWaitStartedAt{0.0f};
    uint64_t framesWhenClientStarted{0};
    uint64_t lastPublishedFrames{0};
    float lastFrameProgressAt{0.0f};
    float nextRecoveryAt{0.0f};
    int activeSceneHandle{-2147483647};
    float fieldOfView{90.0f};
    float positionHalfLife{0.08f};
    float rotationHalfLife{0.12f};
    int width{960};
    int height{540};
    int fps{15};
    int jpegQuality{60};
    int serverPort{5353};
};

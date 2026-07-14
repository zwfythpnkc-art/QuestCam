#pragma once

#include "custom-types/shared/macros.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/Vector3.hpp"

DECLARE_CLASS_CODEGEN(QuestCam, SmoothFrameCapture, UnityEngine::MonoBehaviour) {
  DECLARE_INSTANCE_METHOD(void, OnDestroy);
  DECLARE_CTOR(ctor);

 public:
  void Configure(UnityEngine::RenderTexture* texture, int captureWidth,
                 int captureHeight, int framesPerSecond, int jpegQuality);
  void Capture(UnityEngine::Camera* camera, UnityEngine::Vector3 position,
               UnityEngine::Quaternion rotation, float fieldOfView);

 private:
  UnityW<UnityEngine::RenderTexture> target;
  UnityW<UnityEngine::Texture2D> readableTexture;
  int width{960};
  int height{540};
  int fps{15};
  int quality{60};
  float nextFrameTime{0.0f};
};

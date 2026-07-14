#pragma once

#include "custom-types/shared/macros.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/Texture2D.hpp"

DECLARE_CLASS_CODEGEN(QuestCam, FrameCapture, UnityEngine::MonoBehaviour) {
  DECLARE_INSTANCE_METHOD(void, OnRenderImage,
                          UnityEngine::RenderTexture* source,
                          UnityEngine::RenderTexture* destination);
  DECLARE_INSTANCE_METHOD(void, OnDestroy);
  DECLARE_CTOR(ctor);

 public:
  void Configure(int captureWidth, int captureHeight, int framesPerSecond,
                 int jpegQuality);

 private:
  UnityW<UnityEngine::RenderTexture> target;
  UnityW<UnityEngine::Texture2D> readableTexture;
  int width{960};
  int height{540};
  int fps{15};
  int quality{60};
  float nextFrameTime{0.0f};
};

#pragma once

#include "custom-types/shared/macros.hpp"
#include "UnityEngine/MonoBehaviour.hpp"

#include "System/Array.hpp"

namespace QuestCam {
using AudioSamples = ArrayW<float_t, Array<float_t>*>;
}

DECLARE_CLASS_CODEGEN(QuestCam, AudioCapture, UnityEngine::MonoBehaviour) {
  DECLARE_INSTANCE_METHOD(void, OnAudioFilterRead,
                          QuestCam::AudioSamples data, int32_t channels);
  DECLARE_CTOR(ctor);
};

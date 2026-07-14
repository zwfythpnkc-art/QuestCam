#include "questcam/SettingsUI.hpp"

#include "main.hpp"

#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML-Lite/Creation/Layout.hpp"
#include "bsml/shared/BSML-Lite/Creation/Settings.hpp"
#include "bsml/shared/BSML-Lite/Creation/Text.hpp"

#include "HMUI/ViewController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"

#include <algorithm>
#include <cmath>

namespace QuestCam {
namespace {

bool ReadBool(const char* key, bool fallback) {
  const auto& json = getConfig().config;
  return json.IsObject() && json.HasMember(key) && json[key].IsBool()
             ? json[key].GetBool()
             : fallback;
}

int ReadInt(const char* key, int fallback) {
  const auto& json = getConfig().config;
  return json.IsObject() && json.HasMember(key) && json[key].IsInt()
             ? json[key].GetInt()
             : fallback;
}

float ReadFloat(const char* key, float fallback) {
  const auto& json = getConfig().config;
  return json.IsObject() && json.HasMember(key) && json[key].IsNumber()
             ? json[key].GetFloat()
             : fallback;
}

void WriteBool(const char* key, bool value) {
  auto& configuration = getConfig();
  auto& json = configuration.config;
  if (json.HasMember(key)) {
    json[key].SetBool(value);
  } else {
    json.AddMember(rapidjson::Value(key, json.GetAllocator()).Move(), value,
                   json.GetAllocator());
  }
  configuration.Write();
}

void WriteInt(const char* key, int value) {
  auto& configuration = getConfig();
  auto& json = configuration.config;
  if (json.HasMember(key)) {
    json[key].SetInt(value);
  } else {
    json.AddMember(rapidjson::Value(key, json.GetAllocator()).Move(), value,
                   json.GetAllocator());
  }
  configuration.Write();
}

void WriteFloat(const char* key, float value) {
  auto& configuration = getConfig();
  auto& json = configuration.config;
  if (json.HasMember(key)) {
    json[key].SetFloat(value);
  } else {
    json.AddMember(rapidjson::Value(key, json.GetAllocator()).Move(), value,
                   json.GetAllocator());
  }
  configuration.Write();
}

void DidActivate(HMUI::ViewController* self, bool firstActivation,
                 bool, bool) {
  if (!firstActivation) return;
  auto* container =
      BSML::Lite::CreateScrollableSettingsContainer(self->get_transform());
  auto parent = container->get_transform();

  BSML::Lite::CreateText(
      parent,
      "Fast mode supports Vivify • Wide + Smooth is optional",
      3.2f);
  BSML::Lite::CreateText(parent,
                         "USB/OBS: http://127.0.0.1:5353/", 3.0f);
  BSML::Lite::CreateToggle(parent, "Enable QuestCam", ReadBool("enabled", true),
                           [](bool value) { WriteBool("enabled", value); });
  BSML::Lite::CreateToggle(parent, "Stream game audio",
                           ReadBool("streamAudio", true),
                           [](bool value) { WriteBool("streamAudio", value); });
  BSML::Lite::CreateToggle(
      parent, "Wide + Smooth camera (more GPU)",
      ReadBool("wideSmoothMode", true),
      [](bool value) { WriteBool("wideSmoothMode", value); });
  BSML::Lite::CreateSliderSetting(
      parent, "Field of view", 1.0f, ReadFloat("fieldOfView", 90.0f), 50.0f,
      170.0f, [](float value) { WriteFloat("fieldOfView", value); });
  BSML::Lite::CreateSliderSetting(
      parent, "Position smoothing", 0.01f,
      ReadFloat("positionHalfLife", 0.08f), 0.0f, 0.5f,
      [](float value) { WriteFloat("positionHalfLife", value); });
  BSML::Lite::CreateSliderSetting(
      parent, "Rotation smoothing", 0.01f,
      ReadFloat("rotationHalfLife", 0.12f), 0.0f, 0.5f,
      [](float value) { WriteFloat("rotationHalfLife", value); });
  BSML::Lite::CreateSliderSetting(
      parent, "Stream FPS", 1.0f, static_cast<float>(ReadInt("fps", 15)),
      10.0f, 60.0f,
      [](float value) { WriteInt("fps", static_cast<int>(std::lround(value))); });
  BSML::Lite::CreateSliderSetting(
      parent, "JPEG quality", 1.0f,
      static_cast<float>(ReadInt("jpegQuality", 60)), 35.0f, 85.0f,
      [](float value) {
        WriteInt("jpegQuality", static_cast<int>(std::lround(value)));
      });
  BSML::Lite::CreateSliderSetting(
      parent, "Width", 16.0f, static_cast<float>(ReadInt("width", 960)),
      640.0f, 1920.0f,
      [](float value) { WriteInt("width", static_cast<int>(std::lround(value))); });
  BSML::Lite::CreateSliderSetting(
      parent, "Height", 8.0f, static_cast<float>(ReadInt("height", 540)),
      360.0f, 1080.0f,
      [](float value) { WriteInt("height", static_cast<int>(std::lround(value))); });
}

}  // namespace

void RegisterSettingsUI() {
  BSML::Init();
  BSML::Register::RegisterSettingsMenu("QuestCam", DidActivate, false);
}

}  // namespace QuestCam

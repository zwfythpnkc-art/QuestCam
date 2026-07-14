#include "main.hpp"

#include "questcam/SpectatorCamera.hpp"
#include "questcam/SettingsUI.hpp"
#include "questcam/FrameServer.hpp"

#include "custom-types/shared/register.hpp"
#include "scotland2/shared/modloader.h"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"

#include <algorithm>

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

Configuration& getConfig() {
  static Configuration config(modInfo);
  return config;
}

MOD_EXTERN_FUNC void setup(CModInfo* info) noexcept {
  *info = modInfo.to_c();
  auto& configuration = getConfig();
  configuration.Load();
  auto& json = configuration.config;
  bool changed = false;
  if (!json.IsObject()) {
    json.SetObject();
    changed = true;
  }
  auto& allocator = json.GetAllocator();
  if (!json.HasMember("enabled")) {
    json.AddMember("enabled", true, allocator);
    changed = true;
  }
  if (!json.HasMember("fieldOfView")) {
    json.AddMember("fieldOfView", 90.0f, allocator);
    changed = true;
  }
  if (!json.HasMember("positionHalfLife")) {
    json.AddMember("positionHalfLife", 0.08f, allocator);
    changed = true;
  }
  if (!json.HasMember("rotationHalfLife")) {
    json.AddMember("rotationHalfLife", 0.12f, allocator);
    changed = true;
  }
  if (!json.HasMember("width")) {
    json.AddMember("width", 960, allocator);
    changed = true;
  }
  if (!json.HasMember("height")) {
    json.AddMember("height", 540, allocator);
    changed = true;
  }
  if (!json.HasMember("fps")) {
    json.AddMember("fps", 15, allocator);
    changed = true;
  }
  if (!json.HasMember("jpegQuality")) {
    json.AddMember("jpegQuality", 60, allocator);
    changed = true;
  }
  if (!json.HasMember("serverPort")) {
    json.AddMember("serverPort", 5353, allocator);
    changed = true;
  }
  if (!json.HasMember("streamAudio")) {
    json.AddMember("streamAudio", true, allocator);
    changed = true;
  }
  if (!json.HasMember("wideSmoothMode")) {
    json.AddMember("wideSmoothMode", true, allocator);
    changed = true;
  }
  // Migrate older second-camera installs to the lighter final-frame pipeline.
  // These defaults are intentionally conservative for Quest 3S.
  if (!json.HasMember("capturePipelineVersion")) {
    json.AddMember("capturePipelineVersion", 2, allocator);
    json["width"].SetInt(960);
    json["height"].SetInt(540);
    json["fps"].SetInt(15);
    json["jpegQuality"].SetInt(60);
    changed = true;
  } else if (json["capturePipelineVersion"].IsInt() &&
             json["capturePipelineVersion"].GetInt() < 2) {
    json["capturePipelineVersion"].SetInt(2);
    json["wideSmoothMode"].SetBool(true);
    json["width"].SetInt(960);
    json["height"].SetInt(540);
    json["fps"].SetInt(15);
    json["jpegQuality"].SetInt(60);
    changed = true;
  }
  if (changed) configuration.Write();
  Paper::Logger::RegisterFileContextId(PaperLogger.tag);
  PaperLogger.info("QuestCam {} setup complete", VERSION);
}

MOD_EXTERN_FUNC void late_load() noexcept {
  il2cpp_functions::Init();

  const auto& json = getConfig().config;
  const bool enabled = !json.HasMember("enabled") || !json["enabled"].IsBool() ||
                       json["enabled"].GetBool();
  const int port = json.HasMember("serverPort") && json["serverPort"].IsInt()
                       ? json["serverPort"].GetInt()
                       : 5353;
  if (enabled && !QuestCam::FrameServer::Instance().Start(
                     static_cast<uint16_t>(std::clamp(port, 1024, 65535)),
                     48000, 2)) {
    PaperLogger.error("QuestCam OBS server failed to start");
  }

  custom_types::Register::AutoRegister();
  QuestCam::RegisterSettingsUI();

  auto* host = UnityEngine::GameObject::New_ctor("QuestCam Controller");
  UnityEngine::Object::DontDestroyOnLoad(host);
  host->AddComponent<QuestCam::SpectatorCamera*>();

  PaperLogger.info("QuestCam spectator controller installed");
}

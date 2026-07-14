#include "questcam/AudioCapture.hpp"

#include "questcam/FrameServer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

DEFINE_TYPE(QuestCam, AudioCapture);

namespace QuestCam {

void AudioCapture::ctor() { INVOKE_CTOR(); }

void AudioCapture::OnAudioFilterRead(ArrayW<float_t, Array<float_t>*> data,
                                     int32_t channels) {
  if (!FrameServer::Instance().IsRunning() ||
      !FrameServer::Instance().HasAudioClients() || !data || channels <= 0)
    return;
  std::vector<int16_t> pcm;
  const auto toPcm = [](float sample) {
    const float limited = std::clamp(sample, -1.0f, 1.0f);
    return static_cast<int16_t>(std::lrint(limited * 32767.0f));
  };
  const size_t frameCount = data.size() / static_cast<size_t>(channels);
  pcm.reserve(frameCount * 2);
  for (size_t frame = 0; frame < frameCount; ++frame) {
    const size_t offset = frame * static_cast<size_t>(channels);
    const int16_t left = toPcm(data[offset]);
    const int16_t right = channels > 1 ? toPcm(data[offset + 1]) : left;
    pcm.push_back(left);
    pcm.push_back(right);
  }
  FrameServer::Instance().PublishAudio(std::move(pcm));
}

}  // namespace QuestCam

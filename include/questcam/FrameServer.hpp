#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace QuestCam {

class FrameServer {
 public:
  static FrameServer& Instance();

  bool Start(uint16_t port, int sampleRate, int channels);
  void Stop();
  void PublishJpeg(std::vector<uint8_t> frame);
  void PublishAudio(std::vector<int16_t> samples);
  [[nodiscard]] bool IsRunning() const { return running_; }
  [[nodiscard]] bool HasVideoClients() const { return videoClients_ > 0; }
  [[nodiscard]] bool HasAudioClients() const { return audioClients_ > 0; }
  [[nodiscard]] uint64_t PublishedFrames() const { return publishedFrames_; }
  [[nodiscard]] uint16_t Port() const { return port_; }

 private:
  FrameServer() = default;
  ~FrameServer();
  FrameServer(const FrameServer&) = delete;
  FrameServer& operator=(const FrameServer&) = delete;

  void AcceptLoop();
  void HandleClient(int client);
  void ServePage(int client);
  void ServeVideo(int client);
  void ServeAudio(int client);
  static bool SendAll(int socket, const void* data, size_t size);

  std::atomic<bool> running_{false};
  std::atomic<int> videoClients_{0};
  std::atomic<int> audioClients_{0};
  std::atomic<uint64_t> publishedFrames_{0};
  int listenSocket_{-1};
  uint16_t port_{5353};
  int sampleRate_{48000};
  int channels_{2};
  std::thread acceptThread_;

  std::mutex videoMutex_;
  std::condition_variable videoReady_;
  std::shared_ptr<const std::vector<uint8_t>> latestFrame_;
  uint64_t videoSequence_{0};

  std::mutex audioMutex_;
  std::condition_variable audioReady_;
  std::deque<std::shared_ptr<const std::vector<int16_t>>> audioChunks_;
  uint64_t firstAudioSequence_{0};
  uint64_t nextAudioSequence_{0};
};

}  // namespace QuestCam

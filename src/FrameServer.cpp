#include "questcam/FrameServer.hpp"

#include "main.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <sstream>
#include <string>

namespace QuestCam {
namespace {
constexpr size_t kMaximumAudioChunks = 256;

void WriteLittleEndian16(uint8_t* target, uint16_t value) {
  target[0] = static_cast<uint8_t>(value);
  target[1] = static_cast<uint8_t>(value >> 8);
}

void WriteLittleEndian32(uint8_t* target, uint32_t value) {
  target[0] = static_cast<uint8_t>(value);
  target[1] = static_cast<uint8_t>(value >> 8);
  target[2] = static_cast<uint8_t>(value >> 16);
  target[3] = static_cast<uint8_t>(value >> 24);
}
}  // namespace

FrameServer& FrameServer::Instance() {
  static FrameServer instance;
  return instance;
}

FrameServer::~FrameServer() { Stop(); }

bool FrameServer::Start(uint16_t port, int sampleRate, int channels) {
  if (running_) return true;

  listenSocket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocket_ < 0) {
    PaperLogger.error("OBS server socket() failed: {}", std::strerror(errno));
    return false;
  }

  int reuse = 1;
  setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);
  if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
      listen(listenSocket_, 4) < 0) {
    const int error = errno;
    close(listenSocket_);
    listenSocket_ = -1;
    PaperLogger.error("OBS server could not bind/listen on port {}: {}", port,
                      std::strerror(error));
    return false;
  }

  port_ = port;
  sampleRate_ = sampleRate;
  channels_ = std::clamp(channels, 1, 2);
  running_ = true;
  acceptThread_ = std::thread(&FrameServer::AcceptLoop, this);
  PaperLogger.info("OBS server listening on port {}", port_);
  return true;
}

void FrameServer::Stop() {
  if (!running_.exchange(false)) return;
  if (listenSocket_ >= 0) {
    shutdown(listenSocket_, SHUT_RDWR);
    close(listenSocket_);
    listenSocket_ = -1;
  }
  videoReady_.notify_all();
  audioReady_.notify_all();
  if (acceptThread_.joinable()) acceptThread_.join();
}

void FrameServer::PublishJpeg(std::vector<uint8_t> frame) {
  if (!running_ || frame.empty()) return;
  {
    std::lock_guard lock(videoMutex_);
    latestFrame_ = std::make_shared<const std::vector<uint8_t>>(std::move(frame));
    ++videoSequence_;
    ++publishedFrames_;
  }
  videoReady_.notify_all();
}

void FrameServer::PublishAudio(std::vector<int16_t> samples) {
  if (!running_ || samples.empty()) return;
  {
    std::lock_guard lock(audioMutex_);
    audioChunks_.push_back(
        std::make_shared<const std::vector<int16_t>>(std::move(samples)));
    ++nextAudioSequence_;
    while (audioChunks_.size() > kMaximumAudioChunks) {
      audioChunks_.pop_front();
      ++firstAudioSequence_;
    }
  }
  audioReady_.notify_all();
}

void FrameServer::AcceptLoop() {
  while (running_) {
    const int client = accept(listenSocket_, nullptr, nullptr);
    if (client < 0) {
      if (running_) PaperLogger.warn("OBS server accept failed");
      continue;
    }
    std::thread(&FrameServer::HandleClient, this, client).detach();
  }
}

void FrameServer::HandleClient(int client) {
  std::array<char, 2048> request{};
  const auto count = recv(client, request.data(), request.size() - 1, 0);
  if (count <= 0) {
    close(client);
    return;
  }
  const std::string_view line(request.data(), static_cast<size_t>(count));
  if (line.starts_with("GET /stream.mjpg")) {
    ServeVideo(client);
  } else if (line.starts_with("GET /audio.wav")) {
    ServeAudio(client);
  } else {
    ServePage(client);
  }
  shutdown(client, SHUT_RDWR);
  close(client);
}

void FrameServer::ServePage(int client) {
  static constexpr std::string_view page = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><title>QuestCam</title>
<style>html,body{width:100%;height:100%;margin:0;background:#000;overflow:hidden}img{width:100%;height:100%;object-fit:contain}</style>
</head><body><img src="/stream.mjpg"><audio src="/audio.wav" autoplay></audio></body></html>)HTML";
  std::ostringstream response;
  response << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
              "Cache-Control: no-store\r\nContent-Length: "
           << page.size() << "\r\nConnection: close\r\n\r\n" << page;
  const auto text = response.str();
  SendAll(client, text.data(), text.size());
}

void FrameServer::ServeVideo(int client) {
  ++videoClients_;
  static constexpr std::string_view header =
      "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; "
      "boundary=questcam\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
  if (!SendAll(client, header.data(), header.size())) {
    --videoClients_;
    return;
  }

  uint64_t seen = 0;
  while (running_) {
    std::shared_ptr<const std::vector<uint8_t>> frame;
    {
      std::unique_lock lock(videoMutex_);
      videoReady_.wait_for(lock, std::chrono::seconds(2),
                           [&] { return !running_ || videoSequence_ != seen; });
      if (!running_) break;
      seen = videoSequence_;
      frame = latestFrame_;
    }
    if (!frame) continue;
    std::ostringstream part;
    part << "--questcam\r\nContent-Type: image/jpeg\r\nContent-Length: "
         << frame->size() << "\r\n\r\n";
    const auto partHeader = part.str();
    if (!SendAll(client, partHeader.data(), partHeader.size()) ||
        !SendAll(client, frame->data(), frame->size()) ||
        !SendAll(client, "\r\n", 2)) {
      break;
    }
  }
  --videoClients_;
}

void FrameServer::ServeAudio(int client) {
  ++audioClients_;
  static constexpr std::string_view header =
      "HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\nCache-Control: no-store\r\n"
      "Connection: close\r\n\r\n";
  if (!SendAll(client, header.data(), header.size())) {
    --audioClients_;
    return;
  }

  std::array<uint8_t, 44> wav{};
  std::memcpy(wav.data(), "RIFF", 4);
  WriteLittleEndian32(wav.data() + 4, 0x7fffffffu);
  std::memcpy(wav.data() + 8, "WAVEfmt ", 8);
  WriteLittleEndian32(wav.data() + 16, 16);
  WriteLittleEndian16(wav.data() + 20, 1);
  WriteLittleEndian16(wav.data() + 22, static_cast<uint16_t>(channels_));
  WriteLittleEndian32(wav.data() + 24, static_cast<uint32_t>(sampleRate_));
  WriteLittleEndian32(wav.data() + 28,
                      static_cast<uint32_t>(sampleRate_ * channels_ * 2));
  WriteLittleEndian16(wav.data() + 32, static_cast<uint16_t>(channels_ * 2));
  WriteLittleEndian16(wav.data() + 34, 16);
  std::memcpy(wav.data() + 36, "data", 4);
  WriteLittleEndian32(wav.data() + 40, 0x7fffffffu - 36);
  if (!SendAll(client, wav.data(), wav.size())) {
    --audioClients_;
    return;
  }

  uint64_t next;
  // Keep the WAV response alive even when Unity supplies no samples during a
  // scene transition. Without this, OBS can interpret a quiet two-second gap
  // as the end of the live Media Source and permanently stop its audio.
  const std::vector<int16_t> silence(
      static_cast<size_t>(sampleRate_ * channels_ / 50), 0);
  {
    std::lock_guard lock(audioMutex_);
    next = nextAudioSequence_;
  }
  while (running_) {
    std::vector<std::shared_ptr<const std::vector<int16_t>>> chunks;
    {
      std::unique_lock lock(audioMutex_);
      audioReady_.wait_for(lock, std::chrono::milliseconds(20),
                           [&] { return !running_ || nextAudioSequence_ > next; });
      if (!running_) break;
      if (next < firstAudioSequence_) next = firstAudioSequence_;
      for (; next < nextAudioSequence_; ++next) {
        chunks.push_back(audioChunks_[static_cast<size_t>(next - firstAudioSequence_)]);
      }
    }
    if (chunks.empty() &&
        !SendAll(client, silence.data(), silence.size() * sizeof(int16_t))) {
      --audioClients_;
      return;
    }
    for (const auto& chunk : chunks) {
      if (!SendAll(client, chunk->data(), chunk->size() * sizeof(int16_t))) {
        --audioClients_;
        return;
      }
    }
  }
  --audioClients_;
}

bool FrameServer::SendAll(int socket, const void* data, size_t size) {
  auto* cursor = static_cast<const uint8_t*>(data);
  while (size > 0) {
    const auto sent = send(socket, cursor, size, MSG_NOSIGNAL);
    if (sent <= 0) return false;
    cursor += sent;
    size -= static_cast<size_t>(sent);
  }
  return true;
}

}  // namespace QuestCam

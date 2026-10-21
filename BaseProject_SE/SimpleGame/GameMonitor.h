#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
class RpgGame;

class GameMonitor
{
  public:
    explicit GameMonitor(std::filesystem::path output);
    void Publish(const RpgGame &game);

  private:
    std::filesystem::path destination;
    std::chrono::steady_clock::time_point next{};
    std::uint64_t sequence = 0;
    bool failureReported = false;
};

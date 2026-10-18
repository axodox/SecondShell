#pragma once

namespace SecondShell::Input
{
  //Windows drops injected touch and pen input that is not updated at least every 100 ms, even if it does not move.
  //While started, this calls beat about every 50 ms on a thread of its own; beat repeats the last input if needed.
  class InjectionHeartbeat
  {
    inline static const Axodox::Infrastructure::logger _logger{ "InjectionHeartbeat" };

  public:
    explicit InjectionHeartbeat(std::function<void()> beat);

    void Start();
    void Stop();

  private:
    std::function<void()> _beat;
    std::mutex _mutex;
    std::condition_variable_any _changed;
    bool _isRunning = false;
    std::jthread _thread;  //last: stopped and joined before the rest is destroyed

    void Run(std::stop_token stop);
  };
}

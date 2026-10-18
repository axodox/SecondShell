#include "pch.h"
#include "InjectionHeartbeat.h"

using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Input
{
  const milliseconds _beatInterval{ 50 };

  InjectionHeartbeat::InjectionHeartbeat(function<void()> beat) :
    _beat(move(beat)),
    _thread([this](stop_token stop) { Run(stop); })
  { }

  void InjectionHeartbeat::Start()
  {
    {
      lock_guard lock{ _mutex };
      _isRunning = true;
    }
    _changed.notify_all();
  }

  void InjectionHeartbeat::Stop()
  {
    lock_guard lock{ _mutex };
    _isRunning = false;
  }

  void InjectionHeartbeat::Run(stop_token stop)
  {
    while (true)
    {
      {
        unique_lock lock{ _mutex };
        if (!_changed.wait(lock, stop, [this] { return _isRunning; })) return;
      }

      this_thread::sleep_for(_beatInterval);
      if (stop.stop_requested()) return;

      //Without the lock: beat takes the injector's lock, under which the injector starts and stops this
      try
      {
        _beat();
      }
      catch (const winrt::hresult_error& error)
      {
        _logger.log(log_severity::warning, "Repeating the input failed: {}", winrt::to_string(error.message()));
      }
    }
  }
}

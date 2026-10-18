#pragma once
#include "ITouchInjector.h"
#include "InjectionHeartbeat.h"
#include "InputInjectorProvider.h"

namespace SecondShell::Input
{
  //Touch with the WinRT InputInjector; while fingers are down it repeats the last frame, as Windows needs.
  class TouchInjector : public ITouchInjector
  {
  public:
    explicit TouchInjector(Axodox::Infrastructure::dependency_container* container);
    ~TouchInjector();

    virtual void Inject(std::span<const TouchContact> frame) override;

  private:
    std::shared_ptr<InputInjectorProvider> _provider;

    std::mutex _mutex;
    bool _isInitialized = false;
    std::vector<TouchContact> _downContacts;  //as last injected
    std::chrono::steady_clock::time_point _lastInjection;
    InjectionHeartbeat _heartbeat;  //last: stops before the rest is destroyed

    void Send(std::span<const TouchContact> frame);
    void RepeatIfIdle();
  };
}

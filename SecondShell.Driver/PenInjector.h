#pragma once
#include "IPenInjector.h"
#include "InjectionHeartbeat.h"
#include "InputInjectorProvider.h"

namespace SecondShell::Input
{
  //The pen with the WinRT InputInjector; while it is in range it repeats the last state, as Windows needs.
  class PenInjector : public IPenInjector
  {
  public:
    explicit PenInjector(Axodox::Infrastructure::dependency_container* container);
    ~PenInjector();

    virtual void Inject(const PenState& state) override;

  private:
    std::shared_ptr<InputInjectorProvider> _provider;

    std::mutex _mutex;
    bool _isInitialized = false;
    PenState _state;  //as last injected
    std::chrono::steady_clock::time_point _lastInjection;
    InjectionHeartbeat _heartbeat;  //last: stops before the rest is destroyed

    void Send(const PenState& state, winrt::Windows::UI::Input::Preview::Injection::InjectedInputPointerOptions options);
    void RepeatIfIdle();
  };
}

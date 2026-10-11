#pragma once

namespace SecondShell::Input
{
  //Owns the WinRT InputInjector, which works unpackaged although it is documented as needing the inputInjectionBrokered
  //capability (Design.md, Input injection).
  class InputInjectorProvider
  {
  public:
    InputInjectorProvider();

    bool IsAvailable() const;
    const winrt::Windows::UI::Input::Preview::Injection::InputInjector& Injector() const;

  private:
    winrt::Windows::UI::Input::Preview::Injection::InputInjector _injector;
  };
}

#pragma once
#include "IMouseInjector.h"
#include "InputInjectorProvider.h"

namespace SecondShell::Input
{
  class MouseInjector : public IMouseInjector
  {
  public:
    explicit MouseInjector(Axodox::Infrastructure::dependency_container* container);

    virtual Point Position() const override;
    virtual Point ScreenSize() const override;

    virtual void MoveTo(Point position) override;
    virtual void Press(Actions::MouseButton button) override;
    virtual void Release(Actions::MouseButton button) override;
    virtual void Wheel(float notches, Actions::WheelAxis axis) override;

  private:
    std::shared_ptr<InputInjectorProvider> _provider;

    void Inject(winrt::Windows::UI::Input::Preview::Injection::InjectedInputMouseOptions options, uint32_t data = 0, int32_t x = 0, int32_t y = 0);
  };
}

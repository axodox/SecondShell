#include "pch.h"
#include "InputInjectorProvider.h"

using namespace std;
using namespace winrt::Windows::UI::Input::Preview::Injection;

namespace SecondShell::Input
{
  InputInjectorProvider::InputInjectorProvider() :
    _injector(InputInjector::TryCreate())
  { }

  bool InputInjectorProvider::IsAvailable() const
  {
    return _injector != nullptr;
  }

  const InputInjector& InputInjectorProvider::Injector() const
  {
    if (!_injector) throw runtime_error("Input injection is not available.");
    return _injector;
  }
}

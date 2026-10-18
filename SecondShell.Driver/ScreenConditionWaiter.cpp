#include "pch.h"
#include "ScreenConditionWaiter.h"
#include "TextSearch.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Desktop;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Capture
{
  const milliseconds _frameTimeout{ 2000 };

  namespace
  {
    //False once the wait should end: cancelled, or at the deadline
    bool WaitForNextCheck(steady_clock::time_point deadline, milliseconds interval, const CancellationToken& cancellation)
    {
      auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());
      if (remaining <= 0ms) return false;

      return !cancellation.WaitFor(min(interval, remaining)) && steady_clock::now() < deadline;
    }
  }

  ScreenConditionWaiter::ScreenConditionWaiter(dependency_container* container) :
    _capturer(container->resolve<ScreenCapturer>()),
    _windows(container->resolve<WindowManager>()),
    _quickReader(container->resolve<QuickTextReader>()),
    _thoroughReader(container->resolve<ThoroughTextReader>())
  { }

  TextWait ScreenConditionWaiter::WaitForText(string_view text, const WaitOptions& options, const CancellationToken& cancellation)
  {
    auto deadline = steady_clock::now() + options.Timeout;
    TextSearch search{ text, *_capturer, *_windows, *_quickReader, *_thoroughReader };
    search.Start();
    while (!search.IsFound() && WaitForNextCheck(deadline, options.Interval, cancellation)) search.Check();
    return { search.IsFound(), search.Shown() };
  }

  bool ScreenConditionWaiter::WaitForStable(const Rect& region, milliseconds stableFor, const WaitOptions& options, const CancellationToken& cancellation)
  {
    auto deadline = steady_clock::now() + options.Timeout;
    auto previous = _capturer->Read(region, _frameTimeout);
    auto stableSince = steady_clock::now();
    while (WaitForNextCheck(deadline, options.Interval, cancellation))
    {
      auto current = _capturer->Read(region, _frameTimeout);
      if (current.Bgra != previous.Bgra)
      {
        previous = move(current);
        stableSince = steady_clock::now();
      }
      else if (steady_clock::now() - stableSince >= stableFor)
      {
        return true;
      }
    }
    return false;
  }
}

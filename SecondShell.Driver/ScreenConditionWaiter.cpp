#include "pch.h"
#include "ScreenConditionWaiter.h"

using namespace Axodox::Infrastructure;
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
    _recognizer(container->resolve<TextRecognizer>())
  { }

  TextWait ScreenConditionWaiter::WaitForText(string_view text, const WaitOptions& options, const CancellationToken& cancellation)
  {
    TextWait result;
    auto deadline = steady_clock::now() + options.Timeout;
    do
    {
      result.LastRecognized = _recognizer->Recognize(_capturer->Read(_capturer->Screen(), _frameTimeout));
      result.IsFound = TextRecognizer::Contains(result.LastRecognized, text);
    } while (!result.IsFound && WaitForNextCheck(deadline, options.Interval, cancellation));
    return result;
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

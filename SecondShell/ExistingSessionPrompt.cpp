#include "pch.h"
#include "ExistingSessionPrompt.h"

using namespace SecondShell::Protocol;
using namespace std;

namespace SecondShell::Session
{
  void ExistingSessionPrompt::SetHandler(Handler handler)
  {
    lock_guard lock{ _mutex };
    _handler = move(handler);
  }

  optional<ExistingSessionPolicy> ExistingSessionPrompt::Ask(const ExistingSession& session) const
  {
    Handler handler;
    {
      lock_guard lock{ _mutex };
      handler = _handler;
    }
    return handler ? handler(session) : nullopt;
  }
}

#include "pch.h"
#include "IDriverLink.h"

using namespace SecondShell::Protocol;

namespace SecondShell::Session
{
  IDriverLink::IDriverLink() :
    ActionCompleted(_events)
  { }

  void IDriverLink::OnActionCompleted(const ActionCompletedNotification& notification)
  {
    _events.raise(ActionCompleted, this, notification);
  }
}

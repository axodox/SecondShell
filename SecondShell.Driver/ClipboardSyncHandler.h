#pragma once
#include "ClipboardMirror.h"
#include "DriverProtocol.h"
#include "HostConnection.h"

namespace SecondShell::Desktop
{
  //The session's side of the clipboard sharing in interactive mode: watches the session's clipboard while the host asks
  //(clipboard/sync) and reports its changes (clipboard/changed), and puts the user's clipboard on it (clipboard/set).
  class ClipboardSyncHandler
  {
  public:
    //Adds its handlers to the host connection, between HostConnection::Open and Hello.
    explicit ClipboardSyncHandler(Axodox::Infrastructure::dependency_container* container);

  private:
    std::shared_ptr<Driver::HostConnection> _connection;
    std::shared_ptr<ClipboardMirror> _mirror;
    Axodox::Infrastructure::event_subscription _changedSubscription;

    Rpc::RpcResult<Protocol::EmptyResponse> SetSync(const Protocol::SetClipboardSyncRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::EmptyResponse> Set(const Protocol::SetClipboardRequest& request, const Rpc::CancellationToken& cancellation);
    void OnChanged(ClipboardMirror* sender, const std::vector<Actions::ClipboardContent>& contents);
  };
}

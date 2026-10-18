#include "pch.h"
#include "ClipboardSyncHandler.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Driver;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;

namespace SecondShell::Desktop
{
  namespace
  {
    const JsonRpcError _notAccepted{ JsonRpcErrorCode::InvalidRequest, "The host has not accepted this driver." };
  }

  ClipboardSyncHandler::ClipboardSyncHandler(dependency_container* container) :
    _connection(container->resolve<HostConnection>()),
    _mirror(container->resolve<ClipboardMirror>()),
    _changedSubscription(_mirror->Changed({ this, &ClipboardSyncHandler::OnChanged }))
  {
    _connection->Peer().AddHandler(this, &ClipboardSyncHandler::SetSync);
    _connection->Peer().AddHandler(this, &ClipboardSyncHandler::Set);
  }

  RpcResult<EmptyResponse> ClipboardSyncHandler::SetSync(const SetClipboardSyncRequest& request, const CancellationToken& /*cancellation*/)
  {
    if (!_connection->IsAccepted()) return _notAccepted;

    _mirror->IsWatching(request.IsEnabled);
    return EmptyResponse{};
  }

  RpcResult<EmptyResponse> ClipboardSyncHandler::Set(const SetClipboardRequest& request, const CancellationToken& /*cancellation*/)
  {
    if (!_connection->IsAccepted()) return _notAccepted;

    try
    {
      _mirror->Write(request.Contents);
      return EmptyResponse{};
    }
    catch (const exception& error)
    {
      return JsonRpcError{ JsonRpcErrorCode::InternalError, format("The session's clipboard could not be set: {}", error.what()) };
    }
  }

  void ClipboardSyncHandler::OnChanged(ClipboardMirror* /*sender*/, const vector<ClipboardContent>& contents)
  {
    _connection->Peer().Notify(ClipboardChangedNotification{ contents });
  }
}

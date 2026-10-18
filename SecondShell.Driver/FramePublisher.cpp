#include "pch.h"
#include "FramePublisher.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Driver;
using namespace SecondShell::Graphics;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;

namespace SecondShell::Capture
{
  FramePublisher::FramePublisher(dependency_container* container) :
    _connection(container->resolve<HostConnection>()),
    _capturer(container->resolve<ScreenCapturer>()),
    _frameArrivedSubscription(_capturer->FrameArrived({ this, &FramePublisher::OnFrameArrived }))
  {
    _connection->Peer().AddHandler(this, &FramePublisher::Subscribe);
  }

  RpcResult<SubscribeFramesResponse> FramePublisher::Subscribe(const SubscribeFramesRequest& /*request*/, const CancellationToken& /*cancellation*/)
  {
    if (!_connection->IsAccepted()) return JsonRpcError{ JsonRpcErrorCode::InvalidRequest, "The host has not accepted this driver." };

    //Before the lock: frames arrive with the capturer locked, and take this lock
    auto screen = _capturer->Screen();

    lock_guard lock{ _mutex };
    if (!_host)
    {
      _host.attach(OpenProcess(PROCESS_DUP_HANDLE, FALSE, _connection->HostProcessId()));
      if (!_host) return JsonRpcError{ JsonRpcErrorCode::InternalError, format("The host process cannot be opened: error {}.", GetLastError()) };
    }

    if (!_writer) _writer = make_unique<SharedFrameWriter>(_capturer->Device(), screen.Width, screen.Height);
    return SubscribeFramesResponse{ Share() };
  }

  void FramePublisher::OnFrameArrived(ScreenCapturer* /*sender*/, const CapturedFrame& frame)
  {
    lock_guard lock{ _mutex };
    if (!_host) return;

    //A resolution change: a ring of the new size, which the host opens instead
    if (!_writer || _writer->Width() != frame.Width || _writer->Height() != frame.Height)
    {
      _writer = make_unique<SharedFrameWriter>(_capturer->Device(), frame.Width, frame.Height);
      _connection->Peer().Notify(FramesChangedNotification{ Share() });
      _logger.log(log_severity::information, "Sharing frames of {}x{}.", frame.Width, frame.Height);
    }

    _writer->Write(frame.Context, frame.Texture);
  }

  SharedFrames FramePublisher::Share() const
  {
    auto frames = _writer->ShareWith(_host.get());
    frames.AdapterLuid = _capturer->AdapterLuid();
    return frames;
  }
}

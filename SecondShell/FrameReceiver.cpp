#include "pch.h"
#include "FrameReceiver.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Graphics;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace SecondShell::Session;
using namespace std;

namespace SecondShell::Rendering
{
  FrameReceiver::FrameReceiver(dependency_container* container) :
    _driver(container->resolve<DriverLink>()),
    _sessions(container->resolve<SessionManager>()),
    _framesChangedSubscription(_driver->FramesChanged({ this, &FrameReceiver::OnFramesChanged })),
    _stateChangedSubscription(_sessions->StateChanged({ this, &FrameReceiver::OnStateChanged }))
  { }

  uint64_t FrameReceiver::Generation() const
  {
    return _generation;
  }

  optional<uint64_t> FrameReceiver::AdapterLuid() const
  {
    lock_guard lock{ _mutex };
    if (!_frames) return nullopt;
    return _frames->AdapterLuid;
  }

  unique_ptr<SharedFrameReader> FrameReceiver::CreateReader(ID3D11Device1* device, uint64_t& generation) const
  {
    //Under the lock, so the handles stay open while the reader opens them
    lock_guard lock{ _mutex };
    generation = _generation;
    if (!_frames) return nullptr;
    return make_unique<SharedFrameReader>(device, *_frames);
  }

  void FrameReceiver::OnStateChanged(SessionManager* /*sender*/, SessionState state)
  {
    if (state != SessionState::Ready)
    {
      Replace(nullopt);
      return;
    }

    auto response = _driver->SubscribeFrames();
    if (auto error = get_if<JsonRpcError>(&response))
    {
      _logger.log(log_severity::error, "The session's frames could not be shared: {}", error->Message);
      return;
    }

    Replace(get<SubscribeFramesResponse>(response).Frames);
  }

  void FrameReceiver::OnFramesChanged(DriverLink* /*sender*/, const SharedFrames& frames)
  {
    Replace(frames);
  }

  void FrameReceiver::Replace(optional<SharedFrames> frames)
  {
    //The handles are this process's own, duplicated by the driver
    vector<winrt::handle> handles;
    if (frames)
    {
      for (auto texture : frames->Textures) handles.emplace_back(HANDLE(texture));
      handles.emplace_back(HANDLE(frames->State));
    }

    lock_guard lock{ _mutex };
    _frames = move(frames);
    _handles = move(handles);
    _generation++;
  }
}

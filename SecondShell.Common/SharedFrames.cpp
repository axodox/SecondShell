#include "pch.h"
#include "SharedFrames.h"

using namespace SecondShell::Protocol;
using namespace std;

namespace SecondShell::Graphics
{
  //Three: the host can hold one while the driver writes another, and the newest stays readable
  const uint32_t _textureCount = 3;

  //The host waits this long at most for a texture the driver is writing, which takes it a moment only
  const DWORD _readTimeout = 5;

  namespace
  {
    //Both sides use the same key: the mutex only keeps them from using a texture at the same time
    const UINT64 _key = 0;

    uint64_t Newest(const SharedFrameState* state)
    {
      return atomic_ref<const uint64_t>(state->Newest).load();
    }

    uint64_t Duplicate(HANDLE handle, HANDLE process, DWORD access, DWORD options)
    {
      HANDLE duplicate = nullptr;
      winrt::check_bool(DuplicateHandle(GetCurrentProcess(), handle, process, &duplicate, access, FALSE, options));
      return uint64_t(duplicate);
    }
  }

  SharedFrameWriter::SharedFrameWriter(ID3D11Device* device, uint32_t width, uint32_t height) :
    _width(width),
    _height(height)
  {
    D3D11_TEXTURE2D_DESC description{
      .Width = width,
      .Height = height,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = { 1, 0 },
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX
    };

    for (uint32_t index = 0; index < _textureCount; index++)
    {
      Texture texture;
      winrt::check_hresult(device->CreateTexture2D(&description, nullptr, texture.Resource.put()));
      texture.Mutex = texture.Resource.as<IDXGIKeyedMutex>();
      winrt::check_hresult(texture.Resource.as<IDXGIResource1>()->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, texture.Handle.put()));
      _textures.push_back(move(texture));
    }

    _section.attach(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedFrameState), nullptr));
    winrt::check_bool(bool(_section));
    _state = static_cast<SharedFrameState*>(MapViewOfFile(_section.get(), FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedFrameState)));
    winrt::check_bool(_state != nullptr);
    *_state = {};
  }

  SharedFrameWriter::~SharedFrameWriter()
  {
    if (_state) UnmapViewOfFile(_state);
  }

  uint32_t SharedFrameWriter::Width() const
  {
    return _width;
  }

  uint32_t SharedFrameWriter::Height() const
  {
    return _height;
  }

  SharedFrames SharedFrameWriter::ShareWith(HANDLE process) const
  {
    SharedFrames result{ .Width = _width, .Height = _height };
    for (auto& texture : _textures) result.Textures.push_back(Duplicate(texture.Handle.get(), process, 0, DUPLICATE_SAME_ACCESS));

    //The host only reads the state
    result.State = Duplicate(_section.get(), process, FILE_MAP_READ, 0);
    return result;
  }

  bool SharedFrameWriter::Write(ID3D11DeviceContext* context, ID3D11Texture2D* frame)
  {
    //Not the newest, which the host may be about to read, and not one the host holds
    auto newest = uint32_t(Newest(_state) & 0xFF);
    for (uint32_t offset = 1; offset <= _textureCount; offset++)
    {
      auto index = (newest + offset) % _textureCount;
      auto& texture = _textures[index];
      if (texture.Mutex->AcquireSync(_key, 0) != S_OK) continue;

      D3D11_BOX region{ 0, 0, 0, _width, _height, 1 };
      context->CopySubresourceRegion(texture.Resource.get(), 0, 0, 0, 0, frame, 0, &region);
      texture.Mutex->ReleaseSync(_key);

      atomic_ref<uint64_t>(_state->Newest).store((++_frameNumber << 8) | index);
      return true;
    }
    return false;
  }

  SharedFrameLock::SharedFrameLock(const winrt::com_ptr<ID3D11Texture2D>& texture) :
    _texture(texture)
  { }

  SharedFrameLock::~SharedFrameLock()
  {
    if (_texture) _texture.as<IDXGIKeyedMutex>()->ReleaseSync(_key);
  }

  SharedFrameLock::SharedFrameLock(SharedFrameLock&& other) noexcept :
    _texture(exchange(other._texture, nullptr))
  { }

  SharedFrameLock& SharedFrameLock::operator=(SharedFrameLock&& other) noexcept
  {
    if (this != &other)
    {
      if (_texture) _texture.as<IDXGIKeyedMutex>()->ReleaseSync(_key);
      _texture = exchange(other._texture, nullptr);
    }
    return *this;
  }

  SharedFrameLock::operator bool() const
  {
    return _texture != nullptr;
  }

  ID3D11Texture2D* SharedFrameLock::Texture() const
  {
    return _texture.get();
  }

  SharedFrameReader::SharedFrameReader(ID3D11Device1* device, const SharedFrames& frames) :
    _width(frames.Width),
    _height(frames.Height)
  {
    for (auto handle : frames.Textures)
    {
      winrt::com_ptr<ID3D11Texture2D> texture;
      winrt::check_hresult(device->OpenSharedResource1(HANDLE(handle), IID_PPV_ARGS(texture.put())));
      _textures.push_back(move(texture));
    }

    _state = static_cast<const SharedFrameState*>(MapViewOfFile(HANDLE(frames.State), FILE_MAP_READ, 0, 0, sizeof(SharedFrameState)));
    winrt::check_bool(_state != nullptr);
  }

  SharedFrameReader::~SharedFrameReader()
  {
    if (_state) UnmapViewOfFile(_state);
  }

  uint32_t SharedFrameReader::Width() const
  {
    return _width;
  }

  uint32_t SharedFrameReader::Height() const
  {
    return _height;
  }

  SharedFrameLock SharedFrameReader::TryLockNewer()
  {
    return Lock(true);
  }

  SharedFrameLock SharedFrameReader::LockNewest()
  {
    return Lock(false);
  }

  SharedFrameLock SharedFrameReader::Lock(bool isOnlyNewer)
  {
    auto newest = Newest(_state);
    auto frame = newest >> 8;
    auto index = newest & 0xFF;
    if (frame == 0 || (isOnlyNewer && frame <= _lastFrame) || index >= _textures.size()) return {};

    auto& texture = _textures[index];
    if (texture.as<IDXGIKeyedMutex>()->AcquireSync(_key, _readTimeout) != S_OK) return {};

    _lastFrame = frame;
    return SharedFrameLock{ texture };
  }
}

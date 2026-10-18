#pragma once
#include "DriverProtocol.h"

//The driver's captured frames reach the host without leaving the GPU (Architecture.md 3.6, Findings.md 2.13): a ring of
//textures shared by NT handle, each guarded by a keyed mutex, and a small shared state which tells the newest.
namespace SecondShell::Graphics
{
  //In shared memory: the texture with the newest frame, as frame number << 8 | texture index; 0 before the first.
  struct SharedFrameState
  {
    uint64_t Newest = 0;
  };

  //The driver's side: copies each frame into a texture the host does not hold, then makes it the newest.
  class SECONDSHELL_COMMON_API SharedFrameWriter
  {
  public:
    SharedFrameWriter(ID3D11Device* device, uint32_t width, uint32_t height);
    ~SharedFrameWriter();

    SharedFrameWriter(const SharedFrameWriter&) = delete;
    SharedFrameWriter& operator=(const SharedFrameWriter&) = delete;

    uint32_t Width() const;
    uint32_t Height() const;

    //The handles, duplicated into the process, which owns them then.
    Protocol::SharedFrames ShareWith(HANDLE process) const;

    //False if the host holds every texture, then the frame is skipped.
    bool Write(ID3D11DeviceContext* context, ID3D11Texture2D* frame);

  private:
    struct Texture
    {
      winrt::com_ptr<ID3D11Texture2D> Resource;
      winrt::com_ptr<IDXGIKeyedMutex> Mutex;
      winrt::handle Handle;
    };

    uint32_t _width;
    uint32_t _height;
    std::vector<Texture> _textures;
    winrt::handle _section;
    SharedFrameState* _state = nullptr;
    uint64_t _frameNumber = 0;
  };

  //A frame locked for reading; unlocked when destroyed.
  class SECONDSHELL_COMMON_API SharedFrameLock
  {
  public:
    SharedFrameLock() = default;
    explicit SharedFrameLock(const winrt::com_ptr<ID3D11Texture2D>& texture);
    ~SharedFrameLock();

    SharedFrameLock(SharedFrameLock&& other) noexcept;
    SharedFrameLock& operator=(SharedFrameLock&& other) noexcept;

    explicit operator bool() const;
    ID3D11Texture2D* Texture() const;

  private:
    winrt::com_ptr<ID3D11Texture2D> _texture;
  };

  //The host's side: opens the ring on a device of the same adapter as the driver's.
  class SECONDSHELL_COMMON_API SharedFrameReader
  {
  public:
    //The handles stay the caller's.
    SharedFrameReader(ID3D11Device1* device, const Protocol::SharedFrames& frames);
    ~SharedFrameReader();

    SharedFrameReader(const SharedFrameReader&) = delete;
    SharedFrameReader& operator=(const SharedFrameReader&) = delete;

    uint32_t Width() const;
    uint32_t Height() const;

    //The newest frame, if it is newer than the last one locked; an empty lock otherwise.
    SharedFrameLock TryLockNewer();

    //The newest frame, also if it was locked before, e.g. to draw it again at a new size; empty before the first frame.
    SharedFrameLock LockNewest();

  private:
    uint32_t _width;
    uint32_t _height;
    std::vector<winrt::com_ptr<ID3D11Texture2D>> _textures;
    const SharedFrameState* _state = nullptr;
    uint64_t _lastFrame = 0;

    SharedFrameLock Lock(bool isOnlyNewer);
  };
}

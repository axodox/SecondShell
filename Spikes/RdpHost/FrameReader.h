#pragma once

struct AdapterInfo
{
  LUID Luid{};
  std::string Name;
};

std::vector<AdapterInfo> EnumerateAdapters();

//Spike 2, host side: opens the shared texture and section the writer in the child session duplicated into this process,
//reads frames under the keyed mutex for a while, and reports rate and latency. Saves two frames as PNG.
class FrameReader
{
public:
  FrameReader(Spike::Logger& log, const AdapterInfo& adapter);

  void Run(const std::filesystem::path& framesFile, std::chrono::seconds duration, const std::function<bool()>& stopRequested);

private:
  Spike::Logger& _log;
  AdapterInfo _adapter;
  winrt::com_ptr<ID3D11Device1> _device;
  winrt::com_ptr<ID3D11DeviceContext> _context;

  void SavePng(ID3D11Texture2D* staging, uint32_t width, uint32_t height, const std::filesystem::path& path);
};

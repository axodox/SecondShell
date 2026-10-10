#pragma once
#include "RdpControl.h"

struct SpikeOptions
{
  std::wstring Thread = L"ui";         //ui: RDP control on the WinUI thread; dedicated: on its own STA thread
  HostWindowMode Window = HostWindowMode::Never;
  std::wstring WinUiWindow = L"shown"; //shown (without activation), minimized, hidden
  std::wstring Run;                    //command line started in the child session through RunOnce
  int TimeoutSeconds = 180;
  int Width = 1920, Height = 1080;
  bool Frames = false;                 //spike 2: read shared frames from the child session
  int FrameSeconds = 20;
  int Adapter = 0;                     //DXGI adapter index for the frame reader
  std::wstring Label = L"run";
  int ClassVersion = 9;                //RDP control class: MsRdpClient7 or MsRdpClient9 NotSafeForScripting
  std::wstring Startup = L"runonce";   //runonce or run: the HKCU key that starts the program in the child session

  static SpikeOptions Parse();
  std::string Describe() const;
};

//Runs one spike: arms RunOnce, connects the hidden RDP control to a new child session, writes the session guard file,
//waits for the program in the session to finish, then logs the session off and cleans up.
class SpikeRunner
{
public:
  SpikeRunner(SpikeOptions options, winrt::Microsoft::UI::Dispatching::DispatcherQueue uiQueue, std::function<void(const std::string&)> uiLog);
  ~SpikeRunner();

  void Start(HWND winUiWindow);

private:
  SpikeOptions _options;
  winrt::Microsoft::UI::Dispatching::DispatcherQueue _uiQueue;
  std::function<void(const std::string&)> _uiLog;
  Spike::Logger _log;
  std::thread _worker;
  HWND _winUiWindow = nullptr;

  void Log(const std::string& message);
  void Execute();
};



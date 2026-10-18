#include "pch.h"
#include "GeneralSettings.xaml.h"
#if __has_include("GeneralSettings.g.cpp")
#include "GeneralSettings.g.cpp"
#endif
#include <winrt/Microsoft.UI.Content.h>

using namespace ::SecondShell::Actions;
using namespace ::SecondShell::Environment;
using namespace ::SecondShell::Protocol;
using namespace ::SecondShell::Session;
using namespace ::SecondShell::Setup;
using namespace Axodox::Infrastructure;
using namespace std;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;

namespace winrt::SecondShell::implementation
{
  namespace
  {
    const ImageFormat _formats[] = { ImageFormat::Png, ImageFormat::Jpeg };
    const ShellKind _shells[] = { ShellKind::PowerShell, ShellKind::Pwsh, ShellKind::Cmd, ShellKind::None };

    const wchar_t* FormatName(ImageFormat format)
    {
      return format == ImageFormat::Jpeg ? L"JPEG" : L"PNG";
    }

    const wchar_t* ShellName(ShellKind shell)
    {
      switch (shell)
      {
      case ShellKind::Pwsh: return L"PowerShell 7";
      case ShellKind::Cmd: return L"Command Prompt";
      case ShellKind::None: return L"None: run the program itself";
      default: return L"Windows PowerShell";
      }
    }

    hstring Minutes(uint32_t seconds)
    {
      return seconds % 60 == 0 ? hstring{ format(L"{} min", seconds / 60) } : hstring{ format(L"{} s", seconds) };
    }

    hstring Days(uint32_t days)
    {
      if (days == 0) return L"Forever";
      return days == 1 ? hstring{ L"1 day" } : hstring{ format(L"{} days", days) };
    }

    //The choices, with the current value among them even if it is not one of the usual ones
    template<typename TValue>
    void Show(const ComboBox& box, vector<TValue>& values, const TValue& current, const function<hstring(const TValue&)>& name)
    {
      if (ranges::find(values, current) == values.end()) values.push_back(current);

      box.Items().Clear();
      for (auto& value : values) box.Items().Append(box_value(name(value)));
      box.SelectedIndex(int32_t(ranges::find(values, current) - values.begin()));
    }

    template<typename TValue>
    const TValue& Selected(const ComboBox& box, const vector<TValue>& values)
    {
      return values.at(size_t(max(box.SelectedIndex(), 0)));
    }
  }

  GeneralSettings::GeneralSettings()
  {
    InitializeComponent();
  }

  void GeneralSettings::UseServices(dependency_container* container)
  {
    _settings = container->resolve<SettingsStore>();
    _credentials = container->resolve<CredentialStore>();
    _setupChecker = container->resolve<SetupChecker>();
    _setupLauncher = container->resolve<ElevatedSetupLauncher>();
    _environment = container->resolve<AppEnvironment>();
    Refresh();
  }

  void GeneralSettings::Refresh()
  {
    _isShowing = true;
    auto settings = _settings->Current();

    _resolutions = { { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 } };
    auto resolution = DisplaySettings{ settings.Display.Width, settings.Display.Height };
    Show<DisplaySettings>(ResolutionBox(), _resolutions, resolution, [](auto& display) { return hstring{ format(L"{} × {}", display.Width, display.Height) }; });

    _scales = { 100, 125, 150, 175, 200 };
    Show<uint32_t>(ScaleBox(), _scales, settings.Display.Scale, [](auto& scale) { return hstring{ format(L"{} %", scale) }; });

    _timeouts = { 60, 120, 180, 300, 600 };
    Show<uint32_t>(TimeoutBox(), _timeouts, settings.SignInTimeout, [](auto& seconds) { return Minutes(seconds); });

    FormatBox().Items().Clear();
    for (auto format : _formats) FormatBox().Items().Append(box_value(FormatName(format)));
    FormatBox().SelectedIndex(int32_t(ranges::find(_formats, settings.CaptureFormat) - begin(_formats)));

    InlineLimitBox().Value(double(settings.InlineCaptureLimit));

    CaptureFolderText().Text(_environment->CapturesFolder().c_str());
    _retentions = { 1, 7, 30, 0 };
    Show<uint32_t>(RetentionBox(), _retentions, settings.CaptureRetention, [](auto& days) { return Days(days); });

    ShellBox().Items().Clear();
    for (auto shell : _shells) ShellBox().Items().Append(box_value(ShellName(shell)));
    ShellBox().SelectedIndex(int32_t(ranges::find(_shells, settings.DefaultShell) - begin(_shells)));

    ShowPassword();
    ShowSetup();
    _isShowing = false;
  }

  void GeneralSettings::OnChanged(const IInspectable& /*sender*/, const SelectionChangedEventArgs& /*eventArgs*/)
  {
    Save();
  }

  void GeneralSettings::OnInlineLimitChanged(const NumberBox& /*sender*/, const NumberBoxValueChangedEventArgs& /*eventArgs*/)
  {
    Save();
  }

  void GeneralSettings::Save()
  {
    if (_isShowing || !_settings) return;

    auto& resolution = Selected(ResolutionBox(), _resolutions);
    auto scale = Selected(ScaleBox(), _scales);
    auto timeout = Selected(TimeoutBox(), _timeouts);
    auto format = _formats[max(FormatBox().SelectedIndex(), 0)];
    auto inlineLimit = isnan(InlineLimitBox().Value()) ? 4u : uint32_t(clamp(InlineLimitBox().Value(), 0.0, 20.0));
    auto retention = Selected(RetentionBox(), _retentions);
    auto shell = _shells[max(ShellBox().SelectedIndex(), 0)];

    _settings->Update([&](Settings& settings) {
      settings.Display.Width = resolution.Width;
      settings.Display.Height = resolution.Height;
      settings.Display.Scale = scale;
      settings.SignInTimeout = timeout;
      settings.CaptureFormat = format;
      settings.InlineCaptureLimit = inlineLimit;
      settings.CaptureRetention = retention;
      settings.DefaultShell = shell;
      });
  }

  void GeneralSettings::OnOpenCaptureFolderClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    auto folder = _environment->CapturesFolder();
    error_code error;
    filesystem::create_directories(folder, error);
    ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  }

  void GeneralSettings::OnForgetPasswordClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    _credentials->Delete();
    ShowPassword();
  }

  void GeneralSettings::ShowPassword()
  {
    auto isSaved = _credentials->Load().has_value();
    PasswordTitle().Text(isSaved ? L"Password saved" : L"No password saved");
    PasswordDetail().Text(isSaved ?
      L"In Windows Credential Manager, encrypted for this app; used only to sign the session in" :
      L"The next session asks for it, with the Windows sign-in prompt, and saves it once the sign-in works");
    ForgetPasswordButton().IsEnabled(isSaved);
  }

  void GeneralSettings::ShowSetup()
  {
    auto status = _setupChecker->Status();
    ChildSessionsTitle().Text(status.AreChildSessionsEnabled ? L"Child sessions enabled" : L"Child sessions not enabled");
    ChildSessionsDetail().Text(status.AreChildSessionsEnabled ? L"Set up once for this PC" : L"The agent's desktop needs them; enabling them needs administrator rights once");
    SetupButton().Content(box_value(status.AreChildSessionsEnabled ? L"Check again" : L"Set up…"));
    SetupButton().IsEnabled(!_isSettingUp);

    //A frame interval set by someone else is theirs: SecondShell neither changes nor removes it
    auto isOthers = status.IsSmoothSessionViewOn && !status.IsSmoothSessionViewOurs;
    SmoothViewSwitch().IsOn(status.IsSmoothSessionViewOn);
    SmoothViewSwitch().IsEnabled(!_isSettingUp && !isOthers);
    SmoothViewDetail().Text(isOthers ?
      L"On: DWMFRAMEINTERVAL was set on this PC outside SecondShell, so SecondShell leaves it as it is." :
      L"Every Remote Desktop session on this PC is drawn at 60 fps instead of 30, from its next sign-in; incoming connections too. Changing it needs administrator rights.");
  }

  void GeneralSettings::OnSetupClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    if (_setupChecker->Status().AreChildSessionsEnabled)
    {
      _setupChecker->Check();
      ShowSetup();
      return;
    }

    RunSetup({});
  }

  void GeneralSettings::OnSmoothViewToggled(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    if (_isShowing || _isSettingUp || SmoothViewSwitch().IsOn() == _setupChecker->Status().IsSmoothSessionViewOn) return;
    RunSetup({ SmoothViewSwitch().IsOn() });
  }

  fire_and_forget GeneralSettings::RunSetup(SetupRequest request)
  {
    auto self = get_strong();
    _isSettingUp = true;
    ShowSetup();

    //The launcher waits for the elevated process; not on the UI thread
    auto owner = Microsoft::UI::GetWindowFromWindowId(XamlRoot().ContentIslandEnvironment().AppWindowId());
    auto queue = DispatcherQueue();
    co_await resume_background();
    _setupLauncher->Run(owner, request);
    _setupChecker->Check();
    co_await wil::resume_foreground(queue);

    //Shown as it is now, whether the user approved the prompt or not
    _isSettingUp = false;
    _isShowing = true;
    ShowSetup();
    _isShowing = false;
  }
}

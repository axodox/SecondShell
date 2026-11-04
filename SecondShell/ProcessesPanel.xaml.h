#pragma once
#include "ProcessesPanel.g.h"
#include "ProcessOutputStore.h"

namespace winrt::SecondShell::implementation
{
  //The Processes flyout's content: the processes the agent started with output in this run, newest first, with the
  //selected one's output as it arrives; below them the output files of earlier processes, also of earlier runs, whose
  //end it shows from the file. The status bar refreshes it while the flyout is open; a refresh adds only the new lines,
  //and the repeater builds only the lines on the screen, so even a busy log stays quick.
  struct ProcessesPanel : ProcessesPanelT<ProcessesPanel>
  {
    ProcessesPanel();

    void UseStore(std::shared_ptr<::SecondShell::Agent::ProcessOutputStore> store, std::filesystem::path outputFolder);

    //Shows what changed since the last refresh; with history, also looks at the output folder again.
    void Refresh(bool isHistoryChecked = false);

    std::function<void()> CloseRequested;
    std::function<void(bool isExpanded)> ExpandChanged;

    void OnFilterClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnSearchChanged(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Controls::TextChangedEventArgs& eventArgs);
    void OnProcessClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Controls::ItemClickEventArgs& eventArgs);
    void OnCopyClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnOpenFileClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnExpandClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnCloseClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);

  private:
    //A row of the list: a process of this run, or an earlier output file
    struct Entry
    {
      std::string Key;  //its output file
      std::optional<::SecondShell::Agent::ProcessOutputInfo> Live;
      std::optional<::SecondShell::Agent::ProcessOutputFile> File;
    };

    std::shared_ptr<::SecondShell::Agent::ProcessOutputStore> _store;
    std::filesystem::path _outputFolder;
    uint64_t _version = UINT64_MAX;
    std::vector<::SecondShell::Agent::ProcessOutputFile> _history;
    std::vector<Entry> _entries;  //as listed
    std::optional<std::string> _selected;

    Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable> _lines;
    uint64_t _firstLine = 0;  //the number of _lines[0], for a process of this run
    bool _isExpanded = false;

    void ShowEntries(bool isForced = false);
    bool Matches(const Entry& entry, std::string_view search);
    void ShowLines(bool isReset);
    void ShowDetail();
    void MarkSelected();
    void Select(const std::string& key);
    const Entry* Selected() const;
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct ProcessesPanel : ProcessesPanelT<ProcessesPanel, implementation::ProcessesPanel>
  {
  };
}

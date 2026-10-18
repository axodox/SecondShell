#pragma once
#include "ActionLog.h"

namespace SecondShell::Ui
{
  //A sequence in the action log (docs/UIDesign.md, Log flyout): its title, number, steps, duration and state, then its
  //steps; a capture with its image and file, a command with its output, a failure with its reason.
  class SequenceView
  {
  public:
    using StepFilter = std::function<bool(const Agent::ActionLogEntry& step)>;

    //Null if no step passes the filter.
    static winrt::Microsoft::UI::Xaml::Controls::Border Build(const Agent::SequenceLogEntry& sequence, const StepFilter& filter);

    //The agent's description, or one made from the steps, e.g. "Focus, click, type and 6 more".
    static std::string Title(const Agent::SequenceLogEntry& sequence);
  };
}

#pragma once
#include "ActionLog.h"

namespace SecondShell::Ui
{
  //A sequence in the action log (docs/UIDesign.md, Log flyout): its title, number, steps, duration and state, then its
  //steps; a capture with its image and file, a command with its output, a failure with its reason. Built one row at a
  //time, so the log builds only the rows on the screen: the header and each step are rows, which together look like
  //one box.
  class SequenceView
  {
  public:
    //The top of the box; the first one of the log has no space above it.
    static winrt::Microsoft::UI::Xaml::Controls::Border HeaderRow(const Agent::SequenceLogEntry& sequence, bool isFirst);

    //A step; the last one shown closes the box.
    static winrt::Microsoft::UI::Xaml::Controls::Border StepRow(const Agent::SequenceLogEntry& sequence, const Agent::ActionLogEntry& step, bool isLast);

    //The agent's description, or one made from the steps, e.g. "Focus, click, type and 6 more".
    static std::string Title(const Agent::SequenceLogEntry& sequence);
  };
}

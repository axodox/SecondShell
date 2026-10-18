#pragma once

namespace SecondShell::Integrations
{
  struct DiffLine
  {
    char Mark = ' ';  //'+' added, '-' removed, ' ' context, '~' lines left out
    std::string Text;
  };

  //The lines that differ between two texts, with a few lines around them: for showing a change to a config file.
  //The lines between the first and the last change all count as changed, which suits a change in one place.
  std::vector<DiffLine> ChangedLines(std::string_view before, std::string_view after, size_t context = 2);
}

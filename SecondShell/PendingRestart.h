#pragma once

namespace SecondShell::Setup
{
  //Whether Windows must restart before child sessions work: they fail to start until it does after they were enabled.
  //Kept as the ID of the boot in which they were enabled, in a file of the data folder; a later boot clears it.
  class PendingRestart
  {
    inline static const Axodox::Infrastructure::logger _logger{ "PendingRestart" };

  public:
    explicit PendingRestart(std::filesystem::path file);

    //Child sessions were enabled in this boot; without a boot ID nothing is noted.
    void Note(std::optional<uint32_t> bootId);

    //Whether the boot of the note is still running; a note of an earlier boot is removed.
    bool IsPending(std::optional<uint32_t> bootId);

  private:
    std::filesystem::path _file;
  };

  //Asks Windows to restart, as Start does: applications may still ask to save their work. False if Windows refused.
  bool RestartWindows();
}

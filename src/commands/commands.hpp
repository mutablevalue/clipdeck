#pragma once

namespace clipdeck {

class CommandHandler {
public:
  int Run(int argc, char *argv[]);

private:
  int RunSettingsCommand(int argc, char *argv[]) const;
  int SetKeybindFromCapture() const;
  void PrintHelp() const;
};

} // namespace clipdeck

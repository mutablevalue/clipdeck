#include "commands/commands.hpp"

int main(int argc, char *argv[]) {
  clipdeck::CommandHandler command_handler;
  return command_handler.Run(argc, argv);
}

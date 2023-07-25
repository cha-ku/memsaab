#include "server.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <internal_use_only/config.hpp>

int main(int argc, char **argv)
{
  try {
    CLI::App app{ fmt::format("{} version {}", memsaab::cmake::project_name, memsaab::cmake::project_version) };
    int port_number{ 11211 };
    app.add_option("-p,--port", port_number, "Set port number");
    CLI11_PARSE(app, argc, argv);
    memsaab::server srv(port_number);
    srv.start();
  } catch (const std::exception &e) {
    spdlog::error("Unhandled exception in main: {}", e.what());
  }
  return 0;
}
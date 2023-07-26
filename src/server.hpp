//
// Created by chaku on 25/07/23.
//

#ifndef MEMSAAB_SERVER_HPP
#define MEMSAAB_SERVER_HPP
#include <uvw.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <vector>
#include <unordered_map>

namespace memsaab {

enum class cmd_type {SET, GET, UNKNOWN};
enum class reply {NO, YES};

struct cmd {
  cmd_type type;
  std::string key;
  int expiry_time;
  size_t byte_count;
  reply need_reply;
};

class server {
  int port{ 69069 };
  std::shared_ptr<uvw::tcp_handle> tcp;
  std::shared_ptr<uvw::loop> loop;
  std::string host{"127.0.0.1"};
  std::vector<char> data;
public:
  explicit server(int port_number_) : port(port_number_), loop(uvw::loop::get_default()) {
    tcp = loop->resource<uvw::tcp_handle>();
    tcp->on<uvw::error_event>([](const uvw::error_event &err, uvw::tcp_handle &) {spdlog::error("Connection received error event {}", err.what());});
    tcp->on<uvw::listen_event>([this](const uvw::listen_event &, uvw::tcp_handle &srv) {
      spdlog::info("tcp listen event\n");
      const std::shared_ptr<uvw::tcp_handle> client = srv.parent().resource<uvw::tcp_handle>();
      std::unordered_map<std::string, std::string> storage;
      spdlog::info("client created\n");
      client->on<uvw::close_event>([ptr = srv.shared_from_this()](const uvw::close_event &, uvw::tcp_handle &) {
        spdlog::info("Client close event\n");
        ptr->close();
      });
      auto terminate = std::vector<char>{'\r', '\n'};
      client->on<uvw::data_event>([ptr = srv.shared_from_this(), this, terminate](const uvw::data_event &dataEvent, uvw::tcp_handle &) {
        for(int i = 0; i < dataEvent.length; ++i) {
          if(dataEvent.data[i] != 13) { this->data.emplace_back(dataEvent.data[i]); }
          else {
            std::string str(this->data.begin(), this->data.end());
            fmt::print("Command Received: {}", str);
            this->data.clear();
          }
        }
      });
      client->on<uvw::end_event>([](const uvw::end_event &, uvw::tcp_handle &client) {
        spdlog::info("Client end event\n");
        client.close();
      });
      srv.accept(*client);
      client->read();
    });
    spdlog::info("Starting connection on {}:{}", host, port);
    tcp->bind(host, port);
    tcp->listen();
  }
  ~server() {
    spdlog::info("Closing connection on {}:{}", host, port);
    tcp->close();
    loop->close();
  }

public:
  void start() noexcept;
  cmd_type parse() noexcept;
};
}

#endif// MEMSAAB_SERVER_HPP

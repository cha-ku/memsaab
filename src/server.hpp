//
// Created by chaku on 25/07/23.
//

#ifndef MEMSAAB_SERVER_HPP
#define MEMSAAB_SERVER_HPP
#include <uvw.hpp>
#include <spdlog/spdlog.h>
#include <memory>

namespace memsaab {
class server {
  int port{ 69069 };
  std::shared_ptr<uvw::loop> loop = uvw::loop::get_default();
  std::string host{"127.0.0.1"};
public:
  explicit server(int port_number_) : port(port_number_) {
    const std::shared_ptr<uvw::tcp_handle> tcp = loop->resource<uvw::tcp_handle>();

    tcp->on<uvw::listen_event>([](const uvw::listen_event &, uvw::tcp_handle &srv) {
      const std::shared_ptr<uvw::tcp_handle> client = srv.parent().resource<uvw::tcp_handle>();
      client->on<uvw::close_event>([ptr = srv.shared_from_this()](const uvw::close_event &, uvw::tcp_handle &) { ptr->close(); });
      client->on<uvw::end_event>([](const uvw::end_event &, uvw::tcp_handle &client) { client.close(); });
      srv.accept(*client);
      client->read();
    });

    tcp->bind(host, port);
    spdlog::info("Starting on port {}", port);
    tcp->listen();
  }

public:
  void start() noexcept;
};
}

#endif// MEMSAAB_SERVER_HPP

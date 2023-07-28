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
#include <chrono>
constexpr int PORT = 69069;

namespace memsaab {

enum class cmd_type {SET, GET, UNKNOWN};
enum class reply {NO, YES};

struct storage {
  std::unordered_map<std::string, std::string> key_value;
  std::unordered_map<std::string, std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>> key_expiry;
  storage()=default;

  void add(std::string& key, std::string& val, int expiry_time) {
    key_value[key] = val;
    //https://stackoverflow.com/a/32811935/4063572
    using namespace std::chrono;
    typedef time_point<system_clock, seconds> MyTimePoint;
    auto chrono_expiry_time = time_point_cast<MyTimePoint::duration>(system_clock::time_point(system_clock::now())) += seconds(expiry_time);
    key_expiry[key] = chrono_expiry_time;
  }
};

struct cmd {
  cmd_type type {cmd_type::UNKNOWN};
  std::string key;
  int expiry_time;
  size_t byte_count;
  reply need_reply {reply::YES};

  cmd(cmd_type& type_, std::string& key_, int expiry_time_, size_t& byte_count_, reply& need_reply_) : type(type_), key(std::move(key_)), expiry_time(expiry_time_), byte_count(byte_count_), need_reply(need_reply_) {}

  static void print(const cmd& subject);
};

class server {
  int port{ PORT };
  std::shared_ptr<uvw::tcp_handle> tcp;
  std::shared_ptr<uvw::loop> loop;
  std::string host{"127.0.0.1"};
  std::vector<char> commands;

  static cmd parse(std::string& cmd_str);

public:
  explicit server(const server&)=delete;
  explicit server(server&&)=delete;
  server& operator= (const server&) = delete;
  server& operator= (const server&&) = delete;
  explicit server(int port_number_) : port(port_number_), loop(uvw::loop::get_default()) {
    tcp = loop->resource<uvw::tcp_handle>();
    tcp->on<uvw::error_event>([](const uvw::error_event &err, uvw::tcp_handle &) {spdlog::error("Connection received error event {}", err.what());});
    tcp->on<uvw::listen_event>([this](const uvw::listen_event &, uvw::tcp_handle &srv) {
      spdlog::info("tcp listen event\n");
      const std::shared_ptr<uvw::tcp_handle> client = srv.parent().resource<uvw::tcp_handle>();
      spdlog::info("client created\n");

      auto store = storage();
      auto next_str_is_value = false;

      client->on<uvw::data_event>([ptr = srv.shared_from_this(), this, &next_str_is_value, &store]
        (const uvw::data_event &dataEvent, uvw::tcp_handle &client) {
        for(int i = 0; i < dataEvent.length; ++i) {
          this->commands.emplace_back(dataEvent.data[i]);
        }
        std::string str(this->commands.begin(), this->commands.end());
        auto parsed_cmd = parse(str);
        if (parsed_cmd.type == cmd_type::SET) {
          cmd::print(parsed_cmd);
          next_str_is_value = true;
          this->commands.clear();
        }
        //fmt::print("Command Received: {}", str);
        if (next_str_is_value) {
          store.add(parsed_cmd.key, str, parsed_cmd.expiry_time);
          std::array<char, 6>stored{'S','T','O','R','E','D'};
          client.write(stored.data(), std::size(stored));
        }
      });
      client->on<uvw::end_event>([](const uvw::end_event &, uvw::tcp_handle &client) {
        spdlog::info("Client end event\n");
        client.close();
      });
      client->on<uvw::close_event>([ptr = srv.shared_from_this()](const uvw::close_event &, uvw::tcp_handle &) {
        spdlog::info("Client close event\n");
        ptr->close();
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

  void start() noexcept;
};
}

#endif// MEMSAAB_SERVER_HPP

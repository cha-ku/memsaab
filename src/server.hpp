//
// Created by chaku on 25/07/23.
//

#ifndef MEMSAAB_SERVER_HPP
#define MEMSAAB_SERVER_HPP
#include <memory>
#include <utility>
#include <spdlog/spdlog.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <uvw.hpp>

using namespace std::chrono;
typedef time_point<system_clock, seconds> MyTimePoint;


constexpr int PORT = 69069;

namespace memsaab {

enum class cmd_type {SET, GET, UNKNOWN};
enum class reply {NO, YES};

//key, value, expiry_time
struct key_exp_t
{
  std::string key;
  int expiry_time{0};
};

//END if the key is not found, or VALUE <data block> <flags> <byte count> if the key is found.
struct response_t {
  std::string val;
  std::string key;
  std::string flags;
  std::string byte_count;
};


struct storage_t {
  std::unordered_map<std::string, std::string> key_value;
  std::unordered_map<std::string, std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>> key_expiry;
  storage_t()=default;

  void add(key_exp_t& key_exp, std::string& value) {
    key_value[key_exp.key] = value;
    //https://stackoverflow.com/a/32811935/4063572
    auto chrono_expiry_time = time_point_cast<MyTimePoint::duration>(system_clock::time_point(system_clock::now())) += seconds(key_exp.expiry_time);
    key_expiry[key_exp.key] = chrono_expiry_time;
  }

  std::optional<std::string> get(std::string& key) {
    if(key_value.find(key) != key_value.end()) {
      return key_value[key];
    }
    return {};
  }
};

struct cmd {
  cmd_type type;
  std::string key;
  uint16_t flags;
  int expiry_time;
  unsigned int byte_count;
  reply need_reply;

  cmd(cmd_type& type_, std::string& key_,
    uint16_t flags_, int expiry_time_, unsigned int byte_count_, reply& need_reply_) : type(type_), key(std::move(key_)),
                          flags(flags_), expiry_time(expiry_time_), byte_count(byte_count_), need_reply(need_reply_) {}

  static void print(const cmd& subject);
};

struct resource_handle_t {
  std::string_view last_str;
  key_exp_t storage_val;
  storage_t store;
};

class server {
  int port{ PORT };
  std::shared_ptr<uvw::tcp_handle> tcp;
  std::shared_ptr<uvw::loop> loop;
  std::string host{"127.0.0.1"};
  std::vector<char> commands;
  uint16_t id{0};
  std::unordered_map<std::shared_ptr<uvw::tcp_handle>, resource_handle_t> resourceMap;

  static std::variant<cmd, std::string> parse(std::string& cmd_str);

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
      spdlog::info("client created, id {}\n", fmt::ptr(client));

      resourceMap[client] = resource_handle_t();

      client->on<uvw::data_event>([ptr = srv.shared_from_this(), this]
        (const uvw::data_event &dataEvent, uvw::tcp_handle& clientHandle) mutable {
          auto clientPtr = clientHandle.shared_from_this();
            spdlog::info("client {} data event\n", fmt::ptr(clientPtr));
          auto& resourceHandle = resourceMap[clientPtr] ;

          std::array<char, 8> stored{'S','T','O','R','E','D', '\r', '\n'};
          std::array<char, 5> not_found{'E', 'N','D', '\r', '\n'};
          //printf("data event received %s\n", dataEvent.data.get());
          const auto de_len = dataEvent.length;
          if (de_len > 2 && dataEvent.data[de_len-1] == '\n' && dataEvent.data[de_len-2] == '\r') {
            for(int i = 0; i < dataEvent.length; ++i) {
              this->commands.emplace_back(dataEvent.data[i]);
            }
            std::string str(this->commands.begin(), this->commands.end());
            if (str == (resourceHandle.last_str)) {
              return;
            }
            else {
              resourceHandle.last_str = str;
            }
            auto parsed_cmd_variant = parse(str);
            if (std::holds_alternative<cmd>(parsed_cmd_variant)) {
              auto parsed_cmd = std::get<cmd>(parsed_cmd_variant);
              cmd::print(parsed_cmd);
              //handle set, get and other commands
              if (parsed_cmd.type == cmd_type::SET) {
                resourceHandle.storage_val = key_exp_t();
                //client_resource->storage_val = key_exp_t(parsed_cmd.key, parsed_cmd.expiry_time);
                resourceHandle.storage_val.key = parsed_cmd.key;
                resourceHandle.storage_val.expiry_time = parsed_cmd.expiry_time;
              }
              else if (parsed_cmd.type == cmd_type::GET) {
                auto anOptional = resourceHandle.store.get(parsed_cmd.key);
                if(anOptional.has_value()) {
                  clientHandle.write(anOptional.value().data(), anOptional.value().size());
                }
                else {
                  clientHandle.write(not_found.data(), std::size(not_found));
                }
              }
            }
            else {
              resourceHandle.store.add(resourceHandle.storage_val, std::get<std::string>(parsed_cmd_variant));
              clientHandle.write(stored.data(), std::size(stored));
            }
            this->commands.clear();
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

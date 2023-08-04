//
// Created by chaku on 25/07/23.
//

#ifndef MEMSAAB_SERVER_HPP
#define MEMSAAB_SERVER_HPP
#include <memory>
#include <utility>
#include <spdlog/spdlog.h>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <fmt/chrono.h>
#include <uvw.hpp>

constexpr int PORT = 69069;

namespace memsaab {

enum class CmdType {SET, GET, ADD, REPLACE, APPEND, PREPEND, UNKNOWN};
enum class Reply {NO, YES};
enum class Expiry {ALWAYS, NEVER};

//key, value, expiry_time
struct KeyAttributes
{
  std::string key;
  int expiry_time{0};
  uint16_t flags{0};
  size_t byte_count{0};
};

//END if the key is not found, or VALUE <data block> <flags> <byte count> if the key is found.
struct Response
{
  std::string val;
};


struct Storage
{
  std::unordered_map<std::string, std::string> key_value;
  using ExpiryTimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;
  using ExpiryVariant = std::variant<Expiry, ExpiryTimePoint>;
  std::unordered_map<std::string, ExpiryVariant> key_expiry;
  Storage()=default;

  void remove(std::string& key)
  {
    key_value.erase(key);
    key_expiry.erase(key);
  }

  void set(KeyAttributes& key_exp, const Response& response)
  {
    auto to_add = response.val.substr(0, key_exp.byte_count);
    spdlog::info("Saving {} : {}\n", key_exp.key, to_add);
    key_value[key_exp.key] = to_add;
    //https://stackoverflow.com/a/32811935/4063572
    ExpiryVariant expValue;
    if (key_exp.expiry_time < 0)
    {
      expValue = Expiry::ALWAYS;
    }
    else if (key_exp.expiry_time == 0)
    {
      expValue = Expiry::NEVER;
    }
    else
    {
      using namespace std::chrono;
      auto expiryTime = time_point_cast<ExpiryTimePoint::duration>(system_clock::time_point(system_clock::now())) + seconds(key_exp.expiry_time);
      spdlog::info("Expiry time set to {:%Y-%m-%d %H:%M:%S}\n", expiryTime);
      expValue = expiryTime;
    }
    key_expiry[key_exp.key] = expValue;
  }

  std::optional<Response> get(std::string& key)
  {
    if(key_value.find(key) != key_value.end())
    {
      if(std::holds_alternative<ExpiryTimePoint>(key_expiry[key]))
      {
        auto now = std::chrono::system_clock::now();
        auto expires = std::get<ExpiryTimePoint>(key_expiry[key]);
        if (now > expires)
        {
          spdlog::info("Item expired on {:%Y-%m-%d %H:%M:%S}, current time: {:%Y-%m-%d %H:%M:%S}\n", expires, now);
          remove(key);
          return {};
        }
      }
      else
      {
        auto specialExpiry = std::get<Expiry>(key_expiry[key]);
        if (specialExpiry == Expiry::ALWAYS)
        {
          remove(key);
          return {};
        }
      }
      return Response(key_value[key]);
    }
    return {};
  }
};

struct Cmd
{
  CmdType type;
  KeyAttributes keyAttrs;
  Reply need_reply;

  Cmd(CmdType & type_, std::string& key_,
    uint16_t flags_, int expiry_time_, unsigned int byte_count_,
    Reply & need_reply_) : type(type_), keyAttrs(std::move(KeyAttributes(key_, flags_, expiry_time_, byte_count_))), need_reply(need_reply_) {}

  static void print(const Cmd & subject);
};

struct ResourceHandle
{
  std::string_view last_str;
  KeyAttributes storage_val;
  Storage store;
};

class Server
{
  int port{ PORT };
  std::shared_ptr<uvw::tcp_handle> tcp;
  std::shared_ptr<uvw::loop> loop;
  std::string host{"127.0.0.1"};
  std::vector<char> commands;
  uint16_t id{0};
  std::unordered_map<std::shared_ptr<uvw::tcp_handle>, ResourceHandle> resourceMap;

  static std::variant<Cmd, std::string> parse(std::string& cmd_str);

public:
  explicit Server(const Server &)=delete;
  explicit Server(Server &&)=delete;
  Server & operator= (const Server &) = delete;
  Server & operator= (const Server &&) = delete;
  explicit Server(int port_number_) : port(port_number_), loop(uvw::loop::get_default())
  {
    tcp = loop->resource<uvw::tcp_handle>();
    tcp->on<uvw::error_event>([](const uvw::error_event &err, uvw::tcp_handle &) {spdlog::error("Connection received error event {}", err.what());});
    tcp->on<uvw::listen_event>([this](const uvw::listen_event &, uvw::tcp_handle &srv){
      spdlog::info("tcp listen event\n");
      const std::shared_ptr<uvw::tcp_handle> client = srv.parent().resource<uvw::tcp_handle>();
      spdlog::info("client created, id {}\n", fmt::ptr(client));

      resourceMap[client] = ResourceHandle();

      client->on<uvw::data_event>([ptr = srv.shared_from_this(), this]
        (const uvw::data_event &dataEvent, uvw::tcp_handle& clientHandle) mutable {
          auto clientPtr = clientHandle.shared_from_this();
            spdlog::info("client {} data event\n", fmt::ptr(clientPtr));
          auto& resourceHandle = resourceMap[clientPtr] ;

          std::array<char, 8> stored{'S','T','O','R','E','D', '\r', '\n'};
          std::array<char, 12> notStored{'N', 'O', 'T', '_', 'S','T','O','R','E','D', '\r', '\n'};
          std::array<char, 5> end{'E', 'N','D', '\r', '\n'};
          //printf("data event received %s\n", dataEvent.data.get());
          const auto deLen = dataEvent.length;
          if (deLen > 2 && dataEvent.data[deLen-1] == '\n' && dataEvent.data[deLen-2] == '\r')
          {
            for(int i = 0; i < dataEvent.length; ++i)
            {
              this->commands.emplace_back(dataEvent.data[i]);
            }
            std::string str(this->commands.begin(), this->commands.end());
            if (str == (resourceHandle.last_str))
            {
              return;
            }
            else {
              resourceHandle.last_str = str;
            }
            auto parsedCmdVariant = parse(str);
            if (std::holds_alternative<Cmd>(parsedCmdVariant))
            {
              auto parsedCmd = std::get<Cmd>(parsedCmdVariant);
              Cmd::print(parsedCmd);
              //handle set, get and other commands
              if (parsedCmd.type == CmdType::SET)
              {
                resourceHandle.storage_val = parsedCmd.keyAttrs;
              }
              else if (parsedCmd.type == CmdType::ADD)
              {
                if (resourceHandle.store.key_value.find(parsedCmd.keyAttrs.key) == resourceHandle.store.key_value.end())
                {
                  resourceHandle.storage_val = parsedCmd.keyAttrs;
                }
                else
                {
                  clientHandle.write(notStored.data(), std::size(notStored));
                }
              }
              else if (parsedCmd.type == CmdType::REPLACE)
              {
                if(resourceHandle.store.key_value.find(parsedCmd.keyAttrs.key) != resourceHandle.store.key_value.end())
                {
                  resourceHandle.storage_val = parsedCmd.keyAttrs;
                }
                else
                {
                  clientHandle.write(notStored.data(), std::size(notStored));
                }
              }
              else if (parsedCmd.type == CmdType::GET)
              {
                auto anOptional = resourceHandle.store.get(parsedCmd.keyAttrs.key);
                if(anOptional.has_value())
                {
                  auto& response = anOptional.value();
                  std::string responseStr = "VALUE ";
                  responseStr += response.val + " " + std::to_string(resourceHandle.storage_val.flags) + " " +
                                 std::to_string(std::size(response.val)) + "\r\n";
                  clientHandle.write(responseStr.data(), std::size(responseStr));
                  clientHandle.write(end.data(), std::size(end));
                }
                else
                {
                  clientHandle.write(end.data(), std::size(end));
                }
              }
            }
            else {
              resourceHandle.store.set(resourceHandle.storage_val, Response(std::get<std::string>(parsedCmdVariant)));
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
  ~Server()
  {
    spdlog::info("Closing connection on {}:{}", host, port);
    tcp->close();
    loop->close();
  }

  void start() noexcept;
};
}

#endif// MEMSAAB_SERVER_HPP

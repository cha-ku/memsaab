//
// Created by chaku on 25/07/23.
//
#include "server.hpp"
#include <string>
#include <iterator>

namespace memsaab {
void Server::start() noexcept
{
  loop->run();
}

std::variant<Cmd, std::string> Server::parse(std::string &cmd_str)
{
  std::stringstream cmd_ss(cmd_str);
  const std::istream_iterator<std::string> beg(cmd_ss);
  const std::istream_iterator<std::string> end;
  std::vector<std::string> tokens(beg, end);

  auto type = CmdType::UNKNOWN;
  if(tokens[0] == "set")
  {
    type = CmdType::SET;
  }
  else if (tokens[0] == "get")
  {
    type = CmdType::GET;
  }
  else {
    return tokens[0];
  }

  const auto max_tokens = 6;
  auto is_reply = Reply::YES;

  if (std::size(tokens) == max_tokens && tokens.back() == "noreply")
  {
    is_reply = Reply::NO;
  }

  if(type == CmdType::GET) {
    return Cmd(type, tokens[1], 0, 0, 0, is_reply);
  }

  auto flags = static_cast<uint16_t>(std::stoi(tokens[2]));
  auto exp_time = std::stoi(tokens[3]);
  auto bytes = static_cast<unsigned int>(std::stoi(tokens[4]));
  return Cmd(type, tokens[1], flags, exp_time, bytes, is_reply);
}
void Cmd::print(const Cmd & subject) {
  std::string out;
  out += "type:";
 if(subject.type == CmdType::SET) {
     out += "set";
 }
 else if (subject.type == CmdType::GET) {
     out += "get";
 }
 else {
     out += "unknown";
 }
 out += " key:" + subject.keyAttrs.key;
 out += " flags:" + std::to_string(subject.keyAttrs.flags);
 out += " expiry_time:" + std::to_string(subject.keyAttrs.expiry_time);
 out += " bytes:" + std::to_string(subject.keyAttrs.byte_count);
 out += " Reply:";
 if (subject.need_reply == Reply::YES) {
     out += "yes";
 }
 else {
     out += "no";
 }
 fmt::print("cmd - {}\n", out);
}
}
//
// Created by chaku on 25/07/23.
//
#include "server.hpp"
#include <string>
#include <iterator>

namespace memsaab {
void server::start() noexcept
{
  loop->run();
}

cmd server::parse(std::string &cmd_str)
{
  std::stringstream cmd_ss(cmd_str);
  const std::istream_iterator<std::string> beg(cmd_ss);
  const std::istream_iterator<std::string> end;
  std::vector<std::string> tokens(beg, end);
  const auto max_tokens = 6;
  auto is_reply = reply::YES;
  if (std::size(tokens) == max_tokens && tokens.back() == "noreply")
  {
    is_reply = reply::NO;
  }
  auto type = cmd_type::UNKNOWN;
  if(tokens[0] == "set")
  {
     type = cmd_type::SET;
  }
  else if (tokens[0] == "get")
  {
     type = cmd_type::GET;
  }

  auto exp_time = std::stoi(tokens[2]);
  auto bytes = static_cast<size_t>(std::stoi(tokens[3]));
  return { type, tokens[1], exp_time, bytes, is_reply };
}
void cmd::print(const cmd& subject) {
  std::string out;
  out += " type:";
 if(subject.type == cmd_type::SET) {
     out += "set";
 }
 else if (subject.type == cmd_type::GET) {
     out += "get";
 }
 else {
     out += "unknown";
 }
 out += " key:" + subject.key;
 out += " expiry_time:" + std::to_string(subject.expiry_time);
 out += " bytes:" + std::to_string(subject.byte_count);
 out += " reply:";
 if (subject.need_reply == reply::YES) {
     out += "yes";
 }
 else {
     out += "no";
 }
 fmt::print("cmd - {}", out);
}
}
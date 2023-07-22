#include <CLI/CLI.hpp>
#include <arpa/inet.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <netdb.h>
#include <spdlog/spdlog.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
// #include <internal_use_only/config.hpp>

void perror(std::string_view message) { spdlog::error("error in {}, errno: {}\n", message, errno); }

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) { return &(((struct sockaddr_in *)sa)->sin_addr); }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char **argv)
{
  try {
    CLI::App app;//{ fmt::format("{} version {}", memsaab::cmake::project_name, memsaab::cmake::project_version) };
    int port_number{ 11211 };
    app.add_option("-p,--port", port_number, "Set port number");
    CLI11_PARSE(app, argc, argv);
    spdlog::info("Starting on port {}", port_number);

    // setup for getaddrinfo()

    int status;
    struct addrinfo hints;
    struct addrinfo *res;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;// IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;// TCP stream sockets
    hints.ai_flags = AI_PASSIVE;// for localhost

    if ((status = ::getaddrinfo("localhost", NULL, &hints, &res)) != 0) {
      perror("getaddrinfo");
      spdlog::error("{}", ::gai_strerror(status));
    }

    char ipstr[INET6_ADDRSTRLEN];

    int sockfd;
    for (auto p = res; p != nullptr; p = p->ai_next) {
      // create a socket descriptor s
      sockfd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
      if (sockfd == -1) { perror("socket"); }

      constexpr int yes{ 1 };
      if ((::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) { perror("setsockopt"); }

      // bind socket to the port we passed in
      if (::bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        ::close(sockfd);
        perror("bind");
      }

      break;
    }
    ::freeaddrinfo(res);


    // listen to the port
    constexpr int backlog = 10;
    status = ::listen(sockfd, backlog);
    if (status < 0) { perror("listen"); }

    spdlog::info("Waiting for connections...\n");

    ::sockaddr_storage incoming_address;
    while (1) {
      // accept incoming connection
      ::socklen_t incoming_address_size = sizeof incoming_address;
      auto new_fd = ::accept(sockfd, reinterpret_cast<sockaddr *>(&incoming_address), &incoming_address_size);
      if (new_fd < 0) { perror("accept"); }
      ::inet_ntop(
        incoming_address.ss_family, get_in_addr(reinterpret_cast<sockaddr *>(&incoming_address)), ipstr, sizeof ipstr);
      fmt::print("Server: got connection from {}", std::string(ipstr));
      ::close(new_fd);
    }
  } catch (const std::exception &e) {
    spdlog::error("Unhandled exception in main: {}", e.what());
  }
  return 0;
}
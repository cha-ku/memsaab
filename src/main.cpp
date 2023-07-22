#include <CLI/CLI.hpp>
#include <arpa/inet.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <netdb.h>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
// #include <internal_use_only/config.hpp>

void sigchld_handler(int s)
{
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}


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
    std::string s_port_number = std::to_string(port_number);
    app.add_option("-p,--port", port_number, "Set port number");
    CLI11_PARSE(app, argc, argv);
    spdlog::info("Starting on port {}", port_number);

    // setup for getaddrinfo()

    int getaddrinfo_rv;
    struct addrinfo hints;
    struct addrinfo *res, *p;
    int yes{ 1 };
    int backlog = 10;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;// IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;// TCP stream sockets
    hints.ai_flags = AI_PASSIVE;// for localhost

    if ((getaddrinfo_rv = ::getaddrinfo("localhost", s_port_number.c_str(), &hints, &res)) != 0) {
      perror("getaddrinfo");
      spdlog::error("{}", ::gai_strerror(getaddrinfo_rv));
    }

    char ipstr[INET6_ADDRSTRLEN];

    int sockfd;
    for (p = res; p != nullptr; p = p->ai_next) {
      // create a socket descriptor s
      sockfd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
      if (sockfd == -1) { perror("socket"); }

      if ((::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) { perror("setsockopt"); }

      // bind socket to the port we passed in
      if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        ::close(sockfd);
        perror("bind");
      }

      break;
    }

    ::freeaddrinfo(res);

    if (p == nullptr) { perror("bind"); }

    // listen to the port
    if (::listen(sockfd, backlog) == -1) { perror("listen"); }

    struct sigaction sigact;
    sigact.sa_handler = sigchld_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sigact, nullptr) == -1) { perror("sigaction"); }


    spdlog::info("Waiting for connections...\n");

    ::sockaddr_storage incoming_address;

    while (1) {
      // accept incoming connection
      ::socklen_t incoming_address_size = sizeof incoming_address;
      int new_fd = ::accept(sockfd, (sockaddr *)&incoming_address, &incoming_address_size);
      if (new_fd == -1) { perror("accept"); }
      ::inet_ntop(
        incoming_address.ss_family, get_in_addr(reinterpret_cast<sockaddr *>(&incoming_address)), ipstr, sizeof ipstr);
      fmt::print("Server: got connection from {}\n", std::string(ipstr));

      if (!::fork()) {
        ::close(sockfd);
        if (::send(new_fd, "Hello, world!", 13, 0) == -1) { perror("send"); }
        ::close(new_fd);
        return 0;
      }
      ::close(new_fd);
    }
  } catch (const std::exception &e) {
    spdlog::error("Unhandled exception in main: {}", e.what());
  }
  return 0;
}
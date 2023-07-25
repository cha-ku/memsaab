//
// Created by chaku on 25/07/23.
//
#include "server.hpp"

namespace memsaab {
void server::start() noexcept {
  loop->run();
}
}
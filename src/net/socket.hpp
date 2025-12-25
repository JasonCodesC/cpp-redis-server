#pragma once

#include <cstdint>

namespace net {

int create_listen_socket(uint16_t port, int backlog = 128);
void set_tcp_nodelay(int fd);
void set_reuseaddr(int fd);
void set_nonblocking(int fd);

}

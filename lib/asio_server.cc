/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2014 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
// We wrote this code based on the original code which has the
// following license:
//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "asio_server.h"

#include "asio_server_connection.h"
#include "asio_common.h"
#include "util.h"

namespace nghttp2 {
namespace asio_http2 {
namespace server {

server::server(std::size_t io_context_pool_size,
               std::chrono::microseconds tls_handshake_timeout,
               std::chrono::microseconds read_timeout)
    : io_context_pool_(io_context_pool_size),
      tls_handshake_timeout_(tls_handshake_timeout),
      read_timeout_(read_timeout) {}

boost::system::error_code
server::listen_and_serve(boost::system::error_code &ec,
                         boost::asio::ssl::context *tls_context,
                         const std::string &address, const std::string &port,
                         int backlog, serve_mux &mux, bool asynchronous) {
  ec.clear();

  if (bind_and_listen(ec, address, port, backlog)) {
    return ec;
  }

  for (auto &acceptor : acceptors_) {
    if (tls_context) {
      start_accept(*tls_context, acceptor, mux);
    } else {
      start_accept(acceptor, mux);
    }
  }

  io_context_pool_.run(asynchronous);

  return ec;
}

boost::system::error_code server::bind_and_listen(boost::system::error_code &ec,
                                                  const std::string &address,
                                                  const std::string &port,
                                                  int backlog) {
  // Open the acceptor with the option to reuse the address (i.e.
  // SO_REUSEADDR).
  tcp::resolver resolver(io_context_pool_.executor());
  auto it = resolver.resolve(address, port, ec);
  if (ec) {
    return ec;
  }

  for (const auto &endpoint : it) {
    acceptors_.emplace_back(io_context_pool_.executor());

    if (acceptors_.back().open(endpoint.endpoint().protocol(), ec)) {
      acceptors_.pop_back();
      continue;
    }

    acceptors_.back().set_option(tcp::acceptor::reuse_address(true));

    if (acceptors_.back().bind(endpoint, ec)) {
      acceptors_.pop_back();
      continue;
    }

    if (acceptors_.back().listen(
            backlog == -1 ? boost::asio::socket_base::max_listen_connections : backlog,
            ec)) {
      acceptors_.pop_back();
    }
  }

  if (acceptors_.empty()) {
    return ec;
  }

  // ec could have some errors since we may have failed to bind some
  // interfaces.
  ec.clear();

  return ec;
}

void server::start_accept(boost::asio::ssl::context &tls_context,
                          tcp::acceptor &acceptor, serve_mux &mux) {

  if (!acceptor.is_open()) {
    return;
  }

  auto new_connection = std::make_shared<connection<ssl_socket>>(
      io_context_pool_.executor(), mux, tls_handshake_timeout_, read_timeout_,
      tls_context);

  acceptor.async_accept(
      new_connection->socket().lowest_layer(),
      [this, &tls_context, &acceptor, &mux,
       new_connection](const boost::system::error_code &e) {
        if (!e) {
          new_connection->socket().lowest_layer().set_option(
              tcp::no_delay(true));
          new_connection->start_tls_handshake_deadline();
          new_connection->socket().async_handshake(
              boost::asio::ssl::stream_base::server,
              [new_connection](const boost::system::error_code &e) {
                if (e) {
                  new_connection->stop();
                  return;
                }

                if (!tls_h2_negotiated(new_connection->socket())) {
                  new_connection->stop();
                  return;
                }

                new_connection->start();
              });
        }

        start_accept(tls_context, acceptor, mux);
      });
}

void server::start_accept(tcp::acceptor &acceptor, serve_mux &mux) {

  if (!acceptor.is_open()) {
    return;
  }

  auto new_connection = std::make_shared<connection<tcp::socket>>(
      io_context_pool_.executor(), mux, tls_handshake_timeout_, read_timeout_);

  acceptor.async_accept(
      new_connection->socket(), [this, &acceptor, &mux, new_connection](
                                    const boost::system::error_code &e) {
        if (!e) {
          new_connection->socket().set_option(tcp::no_delay(true));
          new_connection->start_read_deadline();
          new_connection->start();
        }
        if (acceptor.is_open()) {
          start_accept(acceptor, mux);
        }
      });
}

void server::stop() {
  for (auto &acceptor : acceptors_) {
    acceptor.close();
  }
  io_context_pool_.stop();
}

void server::join() { io_context_pool_.join(); }

boost::asio::io_context & server::executor() {
  return io_context_pool_.executor();
}

const std::vector<int> server::ports() const {
  auto ports = std::vector<int>(acceptors_.size());
  auto index = 0;
  for (const auto &acceptor : acceptors_) {
    ports[index++] = acceptor.local_endpoint().port();
  }
  return ports;
}

} // namespace server
} // namespace asio_http2
} // namespace nghttp2

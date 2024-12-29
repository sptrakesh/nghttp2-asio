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
// io_context_pool.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "asio_io_service_pool.h"

namespace nghttp2 {

namespace asio_http2 {

io_context_pool::io_context_pool(std::size_t pool_size) : ioc{static_cast<int>(pool_size)}, concurrency{pool_size} {
  if (pool_size == 0) {
    throw std::runtime_error("io_context_pool size is 0");
  }
}

 io_context_pool::~io_context_pool() {
  stop();
  join();
}


void io_context_pool::run(bool asynchronous) {
  // Create a pool of threads to run handlers posted to the io_context
  threads.reserve(concurrency);
  for (std::size_t i = 0; i < concurrency; ++i) threads.emplace_back([this] {ioc.run();});

  if (!asynchronous) {
    join();
  }
}

void io_context_pool::join() {
  // Wait for all threads in the pool to exit.
  for (auto &thread : threads) if (thread.joinable()) thread.join();
}

void io_context_pool::stop() {
  // Signal the io_context to stop processing.
  ioc.stop();
}

boost::asio::io_context &io_context_pool::executor() {
  return ioc;
}

} // namespace asio_http2

} // namespace nghttp2

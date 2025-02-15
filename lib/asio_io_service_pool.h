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
// io_context_pool.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_io_context_POOL_H
#define ASIO_io_context_POOL_H

#include <thread>
#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/asio/io_context.hpp>

#include <nghttp2/asio_http2.h>

namespace nghttp2 {

namespace asio_http2 {

/// A pool of io_context objects.
class io_context_pool : boost::noncopyable {
public:
  /// Construct the io_context pool.
  io_context_pool() = default;
  ~io_context_pool();

  /// Run all io_context objects in the pool.
  void run(bool asynchronous = false);

  /// Stop the io_context to stop which signals end of work
  void stop();

  /// Join on all io_context threads in the pool.
  void join();

  /// Get the io_context to use.
  boost::asio::io_context &executor();

private:
  /// The io_context to use
  boost::asio::io_context ioc;

  /// Threads to share handlers posted to the io_context
  std::vector<std::thread> threads;
};

} // namespace asio_http2

} // namespace nghttp2

#endif // ASIO_io_context_POOL_H

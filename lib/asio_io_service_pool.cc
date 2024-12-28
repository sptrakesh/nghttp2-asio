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

io_context_pool::io_context_pool(std::size_t pool_size) : next_io_context_(0) {
  if (pool_size == 0) {
    throw std::runtime_error("io_context_pool size is 0");
  }

  // Give all the io_contexts work to do so that their run() functions will not
  // exit until they are explicitly stopped.
  for (std::size_t i = 0; i < pool_size; ++i) {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto work = std::make_shared<boost::asio::executor_work_guard<decltype(boost::asio::io_context().get_executor())>>(io_context->get_executor());
    io_contexts_.push_back(io_context);
    work_.push_back(work);
  }
}

void io_context_pool::run(bool asynchronous) {
  // Create a pool of threads to run all of the io_contexts.
  for (std::size_t i = 0; i < io_contexts_.size(); ++i) {
    futures_.push_back(std::async(std::launch::async,
                                  (size_t(boost::asio::io_context::*)(void)) &
                                      boost::asio::io_context::run,
                                  io_contexts_[i]));
  }

  if (!asynchronous) {
    join();
  }
}

void io_context_pool::join() {
  // Wait for all threads in the pool to exit.
  for (auto &fut : futures_) {
    fut.get();
  }
}

void io_context_pool::force_stop() {
  // Explicitly stop all io_contexts.
  for (auto &iosv : io_contexts_) {
    iosv->stop();
  }
}

void io_context_pool::stop() {
  // Destroy all work objects to signals end of work
  work_.clear();
}

boost::asio::io_context &io_context_pool::get_executor() {
  // Use a round-robin scheme to choose the next io_context to use.
  auto &io_context = *io_contexts_[next_io_context_];
  ++next_io_context_;
  if (next_io_context_ == io_contexts_.size()) {
    next_io_context_ = 0;
  }
  return io_context;
}

const std::vector<std::shared_ptr<boost::asio::io_context>> & io_context_pool::executors() const {
  return io_contexts_;
}

} // namespace asio_http2

} // namespace nghttp2

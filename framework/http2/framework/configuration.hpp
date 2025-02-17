//
// Created by Rakesh on 07/01/2025.
//

#pragma once

#include <thread>
#include <vector>
#include <boost/asio/socket_base.hpp>

namespace spt::http2::framework
{
  struct Configuration
  {
    /// The HTTP method/verb's that are to be sent back in a OPTIONS response.  Customise as appropriate.
    std::vector<std::string> corsMethods{ "DELETE", "GET", "OPTIONS", "PATCH", "POST", "PUT" };

    /// The origins supported by this server instance.
    /// __Note:__ There is no default value for this.  Users __must__ assign domains as appropriate.
    std::vector<std::string> origins;

    /// The hostname to bind to.  Default is to bind on all local hostnames.
    std::string host{ "0.0.0.0" };

    /// The number of server threads for handling requests.  The `boost::asio::io_context` instance
    /// is `run` on the specified number of threads.
    std::size_t numberOfServerThreads{ std::thread::hardware_concurrency() };

    /// The number of worker threads for handling requests.  Client requests are routed
    /// to the configured handler function, which is run on a worker thread pool.  This is
    /// done to offload processing from the server request handling event loop.  Default is
    /// 2x number of CPU cores.
    std::size_t numberOfWorkerThreads{ 2 * std::thread::hardware_concurrency() };

    /// The maximum size of payload a client can submit to an endpoint.  If payload exceeds
    /// this size, server will respond with a `413` status.
    uint32_t maxPayloadSize{ 1024 * 1024 };

    /// The maximum length of the queue of pending incoming connections at the socket level.
    /// Defaults to `boost::asio::socket_base::max_listen_connections`
    int16_t backlog{ boost::asio::socket_base::max_listen_connections };

    /// The port to listen on.  Default 9000.
    uint16_t port{ 9000 };
  };
}
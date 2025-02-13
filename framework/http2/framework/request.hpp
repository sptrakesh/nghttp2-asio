//
// Created by Rakesh on 06/01/2025.
//

#pragma once

#include <nghttp2/asio_http2_server.h>

namespace spt::http2::framework
{
  /// A simple wrapper around the nghttp2-asio request structure.  This structure copies some
  /// of the data in the original request, to ensure that processors have access to data even
  /// if the client has already closed the connection.
  struct Request
  {
    explicit Request( const nghttp2::asio_http2::server::request& req ) :
      header{ req.header() }, method{ req.method() },
      path{ req.uri().path }, query{ req.uri().raw_query },
      remoteEndpoint{ req.remote_endpoint().address().to_string() },
      timestamp{ req.timestamp } {}

    ~Request() = default;
    Request(Request&&) = default;
    Request& operator=(Request&&) = default;

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    nghttp2::asio_http2::header_map header;
    std::string method;
    std::string path;
    std::string query;
    std::string remoteEndpoint;
    decltype(std::chrono::system_clock::now()) timestamp;
  };
}
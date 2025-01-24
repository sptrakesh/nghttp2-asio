//
// Created by Rakesh on 06/01/2025.
//

#pragma once

#include <concepts>
#include <span>

#include <nghttp2/asio_http2_server.h>

namespace spt::http2::framework
{
  template <typename T>
  concept Response = requires( T t, std::span<const std::string> methods, std::span<const std::string> origins )
  {
    /// Construct a response object using any relevant client request headers.
    requires std::constructible_from<T, const nghttp2::asio_http2::header_map&>;
    std::is_same<decltype(T::body), std::string>{};
    std::is_same<decltype(T::filePath), std::string>{};
    std::is_same<decltype(T::status), uint16_t>{};
    /// Set internal state using supported HTTP method/verb and configured origins for server.
    { t.set( methods, origins ) };
  };
}
//
// Created by Rakesh on 06/01/2025.
//

#pragma once

#include "configuration.hpp"
#include "response.hpp"

#include <boost/json/serialize.hpp>
#if defined __has_include
  #if __has_include(<log/NanoLog.hpp>)
    #define HAS_NANO_LOG 1
    #include <log/NanoLog.hpp>
  #endif
#endif

namespace spt::http2::framework
{
  std::string_view statusMessage( uint16_t status );

  void cors( const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res, const Configuration& configuration );

  template <Response Resp>
  Resp error( uint16_t code, std::string_view message, std::span<const std::string> methods,
    const nghttp2::asio_http2::header_map& headers, const Configuration& configuration )
  {
    Resp resp{ headers };
    resp.status = code;
    resp.body = boost::json::serialize( boost::json::object{ { "code", code }, { "cause", message } } );
    resp.set( methods, configuration.origins );
    return resp;
  }

  template <Response Resp>
  Resp error( uint16_t code, std::span<const std::string> methods, const nghttp2::asio_http2::header_map& headers, const Configuration& configuration )
  {
    return error<Resp>( code, statusMessage( code ), methods, headers, configuration );
  }
}
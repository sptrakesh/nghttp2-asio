//
// Created by Rakesh on 06/01/2025.
//

#pragma once

#include "configuration.hpp"
#include "request.hpp"
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
  /**
   * Return a standard text description of an HTTP status code value.
   * @param status The HTTP status code.
   * @return Standard text representation of an HTTP status code.
   */
  std::string_view statusMessage( uint16_t status );

  /**
   * Infer the IP address of the root HTTP client making the request to the server.  The
   * standard `remoteEndpoint` value is unlikely to be the root client due to proxy servers
   * in-between the client and the service.  Use the `X-Real-IP` or `X-Forwarded-For` if available.
   * @param req The request from which the original client IP address is to be determined.
   * @return The inferred IP address of the HTTP client.
   */
  std::string ipaddress( const Request& req );

  /**
   * Respond to an `OPTIONS` HTTP request.  Write an empty `204` response with appropriate headers set.
   * @param req The HTTP request structure.
   * @param res The HTTP response structure to write CORS headers to.
   * @param configuration Configuration structure with information for generating the appropriate CORS headers.
   */
  void cors( const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res, const Configuration& configuration );

  /**
   * Check if the client requested `gzip` compressed response.
   * @param req The HTTP request structure
   * @return Return `true` if the `accept-encoding` header contains the value `gzip`.
   */
  bool shouldCompress( const Request& req );

  /**
   * Check if the client input payload is `gzip` compressed.
   * @param req The HTTP request structure.
   * @return Return `true` if the `content-encoding` header value contains the value `gzip`.
   */
  bool isCompressed( const Request& req );

  /**
   * Generate a standard error response with a JSON body.
   * @tparam Resp The type of response to generate
   * @param code The HTTP status code to respond with.
   * @param message A message describing the cause of the error.
   * @param methods The methods supported for the endpoint which generated this error.
   * @param headers The HTTP *request* headers sent by the client.
   * @param configuration Configuration for generating the appropriate CORS headers.
   * @return The response structure with standard headers and response body content.
   */
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

  /**
   * Generate a standard error response using the default HTTP status description.
   * @tparam Resp The type of response to generate
   * @param code The HTTP status code to respond with.
   * @param methods The methods supported for the endpoint which generated this error.
   * @param headers The HTTP *request* headers sent by the client.
   * @param configuration Configuration for generating the appropriate CORS headers.
   * @return The response structure with standard headers and response body content.
   */
  template <Response Resp>
  Resp error( uint16_t code, std::span<const std::string> methods, const nghttp2::asio_http2::header_map& headers, const Configuration& configuration )
  {
    return error<Resp>( code, statusMessage( code ), methods, headers, configuration );
  }
}
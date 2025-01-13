//
// Created by Rakesh on 06/01/2025.
//

#include "common.hpp"

#include <ranges>

std::string_view spt::http2::framework::statusMessage( uint16_t status )
{
  using std::operator ""sv;

  switch ( status )
  {
  case 100: return "Continue"sv;
  case 101: return "Switching Protocols"sv;
  case 102: return "Processing"sv;
  case 200: return "OK"sv;
  case 201: return "Created"sv;
  case 202: return "Accepted"sv;
  case 203: return "Non-authoritative Information"sv;
  case 204: return "No Content"sv;
  case 205: return "Reset Content"sv;
  case 206: return "Partial Content"sv;
  case 207: return "Multi-Status"sv;
  case 208: return "Already Reported"sv;
  case 226: return "IM Used"sv;
  case 300: return "Multiple Choices"sv;
  case 301: return "Moved Permanently"sv;
  case 302: return "Found"sv;
  case 303: return "See Other"sv;
  case 304: return "Not Modified"sv;
  case 305: return "Use Proxy"sv;
  case 307: return "Temporary Redirect"sv;
  case 308: return "Permanent Redirect"sv;
  case 400: return "Bad Request"sv;
  case 401: return "Unauthorized"sv;
  case 402: return "Payment Required"sv;
  case 403: return "Forbidden"sv;
  case 404: return "Not Found"sv;
  case 405: return "Method Not Allowed"sv;
  case 406: return "Not Acceptable"sv;
  case 407: return "Proxy Authentication Required"sv;
  case 408: return "Request Timeout"sv;
  case 409: return "Conflict"sv;
  case 410: return "Gone";
  case 411: return "Length Required"sv;
  case 412: return "Precondition Failed"sv;
  case 413: return "Payload Too Large"sv;
  case 414: return "Request-URI Too Long"sv;
  case 415: return "Unsupported Media Type"sv;
  case 416: return "Requested Range Not Satisfiable"sv;
  case 417: return "Expectation Failed"sv;
  case 418: return "I'm a teapot"sv;
  case 421: return "Misdirected Request"sv;
  case 422: return "Unprocessable Entity"sv;
  case 423: return "Locked"sv;
  case 424: return "Failed Dependency"sv;
  case 426: return "Upgrade Required"sv;
  case 428: return "Precondition Required"sv;
  case 429: return "Too Many Requests"sv;
  case 431: return "Request Header Fields Too Large"sv;
  case 444: return "Connection Closed Without Response"sv;
  case 451: return "Unavailable For Legal Reasons"sv;
  case 499: return "Client Closed Request"sv;
  case 500: return "Internal Server Error"sv;
  case 501: return "Not Implemented"sv;
  case 502: return "Bad Gateway"sv;
  case 503: return "Service Unavailable"sv;
  case 504: return "Gateway Timeout"sv;
  case 505: return "HTTP Version Not Supported"sv;
  case 506: return "Variant Also Negotiates"sv;
  case 507: return "Insufficient Storage"sv;
  case 508: return "Loop Detected"sv;
  case 510: return "Not Extended"sv;
  case 511: return "Network Authentication Required"sv;
  case 599: return "Network Connect Timeout Error"sv;
  default:
    return "Undefined"sv;
  }
}

void spt::http2::framework::cors( const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res, const Configuration& configuration )
{
  auto hdrs = nghttp2::asio_http2::header_map{
      { "Access-Control-Allow-Methods", { std::format( "{:n:}", configuration.corsMethods ), false } },
      { "Access-Control-Allow-Headers", { "*, authorization", false } },
      { "content-length", { "0", false } }
  };

  auto iter = req.header().find( "origin" );
  if ( iter == std::cend( req.header() ) ) iter = req.header().find( "Origin" );
  if ( iter != std::cend( req.header() ) )
  {
    const auto oiter = std::ranges::find( configuration.origins, iter->second.value );
    if ( oiter != std::ranges::end( configuration.origins ) )
    {
      hdrs.emplace( "Access-Control-Allow-Origin", nghttp2::asio_http2::header_value{ iter->second.value, false } );
      hdrs.emplace( "Vary", nghttp2::asio_http2::header_value{ "Origin", false } );
    }
#ifdef HAS_NANO_LOG
    else LOG_WARN << "Origin " << iter->second.value << " not allowed";
#endif
  }

  res.write_head( 200, std::move( hdrs ) );
  res.end( "" );
}

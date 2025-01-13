#include <charconv>
#include <boost/asio/signal_set.hpp>
#include <http2/framework/server.hpp>
#include <log/NanoLog.hpp>

namespace
{
  namespace presponse
  {
    struct Response
    {
      Response( const nghttp2::asio_http2::header_map& headers )
      {
        auto iter = headers.find( "origin" );
        if ( iter == std::cend( headers ) ) iter = headers.find( "Origin" );
        if ( iter == std::cend( headers ) ) return;
        origin = iter->second.value;
      }

      ~Response() = default;
      Response(Response&&) = default;
      Response& operator=(Response&&) = default;

      Response(const Response&) = delete;
      Response& operator=(const Response&) = delete;

      void set( std::span<const std::string> methods, std::span<const std::string> origins )
      {
        headers = nghttp2::asio_http2::header_map{
            { "Access-Control-Allow-Methods", { std::format( "{:n:}", methods ), false } },
            { "Access-Control-Allow-Headers", { "*, authorization", false } },
            { "content-type", { "application/json; charset=utf-8", false } },
            { "content-length", { std::to_string( body.size() ), false } }
        };
        if ( compressed )
        {
          headers.emplace( "content-encoding", nghttp2::asio_http2::header_value{ "gzip", false } );
        }

        if ( origin.empty() ) return;
        const auto iter = std::ranges::find( origins, origin );
        if ( iter != std::ranges::end( origins ) )
        {
          headers.emplace( "Access-Control-Allow-Origin", nghttp2::asio_http2::header_value{ origin, false } );
          headers.emplace( "Vary", nghttp2::asio_http2::header_value{ "Origin", false } );
        }
        else LOG_WARN << "Origin " << origin << " not allowed";
      }

      nghttp2::asio_http2::header_map headers;
      std::string body{ "{}" };
      std::string entity;
      std::string correlationId;
      std::string filePath;
      std::string origin;
      uint16_t status{ 200 };
      bool compressed{ false };
    };
  }
}

int main()
{
  using namespace spt::http2::framework;

  nanolog::set_log_level( nanolog::LogLevel::DEBUG );
  nanolog::initialize( nanolog::GuaranteedLogger(), "/tmp/", "nghttp2-asio-example", true );

  auto configuration = Configuration{};
  configuration.port = 3000;
  configuration.origins = {"http://localhost:3000"};
  configuration.corsMethods = {"GET", "OPTIONS"};

  auto server = Server<presponse::Response>{ configuration };

  server.addHandler( "GET", "/", []( const RoutingRequest& rr, const auto& )
  {
    auto resp = presponse::Response{ rr.req.header };
    resp.headers.emplace( "content-type", "application/json; charset=utf-8" );
    resp.body = boost::json::serialize( boost::json::object{
      { "status", 200 },
      { "message", "ok" }
    } );
    resp.headers.emplace( "content-length", std::to_string( resp.body.size() ) );
    return resp;
  } );

  server.addHandler( "GET", "/*", []( const RoutingRequest& rr, const auto& params )
  {
    auto resp = presponse::Response{ rr.req.header };
    resp.headers.emplace( "content-type", "application/json; charset=utf-8" );
    resp.body = boost::json::serialize( boost::json::object{
      { "status", 200 },
      { "message", "ok" },
      { "resource", params.at( "_wildcard_" ) }
    } );
    resp.headers.emplace( "content-length", std::to_string( resp.body.size() ) );
    return resp;
  } );

  LOG_INFO << "Starting HTTP/2 server on localhost:3000";
  server.start();
  LOG_INFO << "Test with curl --http2-prior-knowledge http://localhost:3000/";
  LOG_INFO << "Test with curl --http2-prior-knowledge http://localhost:3000/some/resource.html";
  LOG_INFO << "CTRL+C to exit";

  auto ioc = boost::asio::io_context{};
  boost::asio::signal_set signals( ioc, SIGINT, SIGTERM );
  signals.async_wait( [&server](auto const&, int ) { server.stop(); });

  ioc.run();
}

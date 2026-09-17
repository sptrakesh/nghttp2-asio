//
// Created by Victor on 15/09/2026.
//

#include <http2/framework/server.hpp>

#include <atomic>
#include <chrono>
#include <format>
#include <future>
#include <thread>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <nghttp2/asio_http2_client.h>

using std::operator ""s;
using std::operator ""sv;

namespace
{
  namespace pdisconnect
  {
    // The client must give up well after its connection is established but well before the
    // handler returns.  Too tight a disconnect races the client library's own connect path, where
    // session_impl::connected() sets TCP_NODELAY on what may already be a closed socket.
    constexpr auto handlerDelay = std::chrono::milliseconds{ 1000 };
    constexpr auto disconnectAfter = std::chrono::milliseconds{ 200 };

    struct Response
    {
      explicit Response( const nghttp2::asio_http2::header_map& ) {}

      ~Response() = default;
      Response(Response&&) = default;
      Response& operator=(Response&&) = default;

      Response(const Response&) = delete;
      Response& operator=(const Response&) = delete;

      void set( std::span<const std::string> methods, std::span<const std::string> )
      {
        headers = nghttp2::asio_http2::header_map{
            { "Access-Control-Allow-Methods", { std::format( "{:n:}", methods ), false } },
            { "content-type", { "text/plain", false } },
            { "content-length", { std::to_string( body.size() ), false } }
        };
      }

      nghttp2::asio_http2::header_map headers;
      std::string body{ "{}" };
      std::string entity;
      std::string correlationId;
      std::string filePath;
      uint16_t status{ 200 };
      bool compressed{ false };
    };

    struct Fixture
    {
      Fixture() : server{ config } { setup(); }
      ~Fixture() { server.stop(); }

      // Number of slow handlers that ran to completion.  Asserted on below so a green run cannot
      // mean the request was dropped before commit() -- that would test nothing.
      static std::atomic_uint32_t completed;

    private:
      void setup()
      {
        server.addHandler( "GET"sv, "/ping"sv, []( const spt::http2::framework::RoutingRequest& rr, auto&& )
        {
          auto resp = Response( rr.req.header );
          resp.headers.emplace( "content-type", "text/plain" );
          resp.body = "pong"s;
          resp.status = 200;
          return resp;
        });

        server.addHandler( "GET"sv, "/slow"sv, []( const spt::http2::framework::RoutingRequest& rr, auto&& )
        {
          std::this_thread::sleep_for( handlerDelay );
          auto resp = Response( rr.req.header );
          resp.headers.emplace( "content-type", "text/plain" );
          resp.body = "slow"s;
          resp.status = 200;
          completed.fetch_add( 1 );
          return resp;
        });

        server.start();
      }

      static spt::http2::framework::Configuration create()
      {
        spt::http2::framework::Configuration config;
        config.port = 9001;
        config.readTimeout = std::chrono::seconds( 5 );
        return config;
      }

      spt::http2::framework::Configuration config{ create() };
      mutable spt::http2::framework::Server<Response> server;
    };

    std::atomic_uint32_t Fixture::completed{ 0 };

    // Submits a request and gives up before the server can respond, driven by the client read
    // timeout -- the same thing any HTTP client does when the server is slower than its timeout.
    // The session tears itself down through the library's own path, so the server sees a normal
    // client disconnect.
    void disconnectEarly( std::string_view path )
    {
      boost::asio::io_context ioc;
      auto s = nghttp2::asio_http2::client::session{ ioc, "localhost", "9001" };
      s.read_timeout( disconnectAfter );

      s.on_connect( [&s, path]( const boost::asio::ip::tcp::endpoint& )
      {
        boost::system::error_code ec;
        s.submit( ec, "GET", std::format( "http://localhost:9001{}", path ) );
        if ( ec ) LOG_WARN << ec.message();
      } );

      s.on_error( []( const boost::system::error_code& ) { /* expected -- we timed out */ } );

      ioc.run();
    }

    std::string get( std::string_view path )
    {
      boost::asio::io_context ioc;
      auto response = std::string{};

      auto s = nghttp2::asio_http2::client::session{ ioc, "localhost", "9001" };
      s.read_timeout( std::chrono::seconds{ 5 } );
      s.on_connect( [&s, &response, path]( const boost::asio::ip::tcp::endpoint& )
      {
        boost::system::error_code ec;
        auto req = s.submit( ec, "GET", std::format( "http://localhost:9001{}", path ) );
        if ( ec )
        {
          LOG_WARN << ec.message();
          return;
        }

        req->on_response( [&response]( const nghttp2::asio_http2::client::response& res )
        {
          res.on_data( [&response]( const uint8_t* data, std::size_t length )
          {
            response.append( reinterpret_cast<const char*>( data ), length );
          } );
        } );

        req->on_close( [&s]( uint32_t ) { s.shutdown(); } );
      } );

      ioc.run();
      return response;
    }
  }
}

// Stream::commit used to call res.executor() to find the strand to post to, but nghttp2 destroys
// the response once the close callback fires, so a client that disconnected before the handler
// finished took the whole process down with SIGSEGV.
TEST_CASE_PERSISTENT_FIXTURE( pdisconnect::Fixture, "Client disconnect while handler is running", "[disconnect]" )
{
  GIVEN( "A server with a handler slower than the client is willing to wait" )
  {
    WHEN( "A single client disconnects before the response is written" )
    {
      const auto before = completed.load();
      pdisconnect::disconnectEarly( "/slow" );
      std::this_thread::sleep_for( pdisconnect::handlerDelay * 3 );

      THEN( "The handler still ran to completion against the departed client" )
      {
        REQUIRE( completed.load() > before );
      }

      AND_THEN( "The server survives and still serves requests" )
      {
        CHECK( pdisconnect::get( "/ping" ) == "pong" );
      }
    }

    AND_WHEN( "Many clients disconnect concurrently before their responses are written" )
    {
      constexpr auto total = 16;
      const auto before = completed.load();
      auto vec = std::vector<std::future<void>>{};
      vec.reserve( total );

      for ( auto i = 0; i < total; ++i )
      {
        vec.push_back( std::async( std::launch::async, []{ pdisconnect::disconnectEarly( "/slow" ); } ) );
      }
      for ( auto& fut : vec ) fut.get();

      std::this_thread::sleep_for( pdisconnect::handlerDelay * 4 );

      // Deliberately not an exact count: process() drops any request whose client is already gone
      // by the time a worker picks it up, so under load some never reach the handler at all.  The
      // ones that do are what exercise commit() against a dead response.
      THEN( "Handlers ran to completion against departed clients" )
      {
        REQUIRE( completed.load() > before );
      }

      AND_THEN( "The server survives and still serves requests" )
      {
        CHECK( pdisconnect::get( "/ping" ) == "pong" );
      }
    }

    AND_WHEN( "A client disconnects and a well behaved client requests the same route" )
    {
      pdisconnect::disconnectEarly( "/slow" );

      THEN( "The slow route still responds normally" )
      {
        CHECK( pdisconnect::get( "/slow" ) == "slow" );
      }
    }
  }
}

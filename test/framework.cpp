//
// Created by Rakesh on 07/01/2025.
//

#include <http2/framework/server.hpp>

#include <array>
#include <future>
#include <ranges>
#include <boost/json/parse.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nghttp2/asio_http2_client.h>

using std::operator ""s;
using std::operator ""sv;

namespace
{
  namespace ptest
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

    Response get( const spt::http2::framework::RoutingRequest& rr, const boost::container::flat_map<std::string_view, std::string_view>& params )
    {
      auto resp = Response( rr.req.header );
      resp.headers.emplace( "content-type", "application/json" );

      resp.body = boost::json::serialize( boost::json::object{
        { "status", 200 },
        { "message", "ok" },
        { "parameter", params.at( "slug" ) }
      } );
      resp.correlationId = params.at( "slug" );
      resp.headers.emplace( "content-length", std::to_string( resp.body.size() ) );
      return resp;
    }

    Response post( const spt::http2::framework::RoutingRequest& rr, boost::container::flat_map<std::string_view, std::string_view>&& )
    {
      auto resp = Response( rr.req.header );
      resp.headers.emplace( "content-type", "application/json" );
      if ( rr.body.empty() )
      {
        resp.status = 400;
        resp.body = boost::json::serialize( boost::json::object{ { "status", resp.status }, { "message", "Empty body" } } );
        return resp;
      }

      auto ec = boost::system::error_code{};
      auto parsed = boost::json::parse( rr.body, ec );
      if ( ec )
      {
        resp.status = 400;
        resp.body = boost::json::serialize( boost::json::object{ { "status", resp.status }, { "message", "Invalid json" } } );
        LOG_WARN << "Error parsing JSON " << ec.message();
        return resp;
      }

      if ( !parsed.is_object() )
      {
        resp.status = 400;
        resp.body = boost::json::serialize( boost::json::object{ { "status", resp.status }, { "message", "Not json object" } } );
        LOG_WARN << "Expected JSON object";
        return resp;
      }

      resp.body = boost::json::serialize( boost::json::object{
        { "received", std::chrono::system_clock::now().time_since_epoch().count() },
        { "server", "nghttp2::asio::server::framework" },
        { "contentLength", rr.body.size() },
        { "payload", parsed.as_object() }
      } );
      resp.headers.emplace( "content-length", std::to_string( resp.body.size() ) );

      return resp;
    }

    struct Fixture
    {
      Fixture() : server{ config } { setup(); }
      ~Fixture() { server.stop(); }

    private:
      void setup()
      {
        server.addHandler( "GET"sv, "/"sv, []( const spt::http2::framework::RoutingRequest& rr, auto&& )
        {
          auto resp = Response( rr.req.header );
          resp.headers.emplace( "content-type", "text/plain" );
          resp.body = "Ok"s;
          resp.status = 200;
          resp.entity = "/"s;
          return resp;
        });

        server.addHandler( "GET"sv, "/data/{slug}", &get );
        server.addHandler( "POST"sv, "/input"sv, &post );

        server.start();
      }

      spt::http2::framework::Configuration config;
      mutable spt::http2::framework::Server<Response> server;
    };

    std::tuple<std::string, std::string> response( std::string_view path )
    {
      using O = std::tuple<std::string, std::string>;
      boost::asio::io_context ioc;

      auto response = std::string{};
      response.reserve( 1024 );
      auto ct = std::string{};

      auto s = nghttp2::asio_http2::client::session{ ioc, "localhost", "9000" };
      s.read_timeout( std::chrono::seconds{ 5 } );
      s.on_connect( [&s, &response, &ct, path]( const boost::asio::ip::tcp::endpoint& )
      {
        boost::system::error_code ec;
        auto req = s.submit( ec, "GET", std::format( "http://localhost:3000{}", path ) );
        if ( ec )
        {
          LOG_WARN << ec.message();
          return;
        }

        req->on_response( [&response, &ct]( const nghttp2::asio_http2::client::response& res )
        {
          if ( auto iter = res.header().find( "content-type" ); iter != res.header().end() ) ct = iter->second.value;
          res.on_data( [&response]( const uint8_t* data, std::size_t length )
          {
            response.append( reinterpret_cast<const char*>( data ), length );
          } );
        } );

        req->on_close( [&s]( uint32_t ) { s.shutdown(); } );
      } );

      ioc.run();
      return O{ ct, response };
    }

    std::tuple<std::string, std::string> response( std::string_view path, const std::string& data )
    {
      using O = std::tuple<std::string, std::string>;
      boost::asio::io_context ioc;

      auto response = std::string{};
      response.reserve( 1024 );
      auto ct = std::string{};

      auto s = nghttp2::asio_http2::client::session{ ioc, "localhost", "9000" };
      s.read_timeout( std::chrono::seconds{ 5 } );
      s.on_connect( [&s, &response, &ct, path, &data]( const boost::asio::ip::tcp::endpoint& )
      {
        boost::system::error_code ec;
        auto start = std::make_shared<std::size_t>( 0 );
        auto req = s.submit( ec, "POST", std::format( "http://localhost:3000{}", path ),
            [&data, start]( uint8_t* buf, size_t len, uint32_t* data_flags ) -> nghttp2::asio_http2::generator_cb::result_type
            {
              if ( *start >= data.size() )
              {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                return 0;
              }

              const auto n = std::min<size_t>( len, data.size() - *start );
              std::memcpy( buf, data.data() + *start, n );
              *start += n;
              if ( *start < data.size() )
              {
                *data_flags |= NGHTTP2_ERR_DEFERRED;
              }
              else
              {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
              }

              return n;
            },
            { { "content-length", { std::to_string( data.size() ), false } },
                { "content-type", { "application/json", false } } } );
        if ( ec )
        {
          LOG_WARN << ec.message();
          return;
        }

        req->on_response( [&response, &ct]( const nghttp2::asio_http2::client::response& res )
        {
          if ( auto iter = res.header().find( "content-type" ); iter != res.header().end() ) ct = iter->second.value;
          res.on_data( [&response]( const uint8_t* data, std::size_t length )
          {
            response.append( reinterpret_cast<const char*>( data ), length );
          } );
        } );

        req->on_close( [&s]( uint32_t ) { s.shutdown(); } );
      } );

      ioc.run();
      return O{ ct, response };
    }
  }
}

TEST_CASE_PERSISTENT_FIXTURE( ptest::Fixture, "Testing server using framework", "[framework]" )
{
  GIVEN( "A server running on localhost:9000" )
  {
    WHEN( "Making get request to root path" )
    {
      const auto [ct, resp] = ptest::response( "/" );
      CHECK( ct == "text/plain" );
      CHECK( resp == "Ok" );
    }

    AND_WHEN( "Making get request to data path" )
    {
      const auto [ct, resp] = ptest::response( "/data/blah" );
      CHECK( ct == "application/json" );
      REQUIRE_FALSE( resp.empty() );

      auto ec = boost::system::error_code{};
      auto parsed = boost::json::parse( resp, ec );
      REQUIRE_FALSE( ec );
      REQUIRE( parsed.is_object() );
      REQUIRE( parsed.as_object().contains( "status" ) );
      REQUIRE( parsed.as_object().at( "status" ).is_number() );
      REQUIRE( parsed.as_object().at( "status" ).as_int64() == 200 );
      REQUIRE( parsed.as_object().contains( "message" ) );
      REQUIRE( parsed.as_object().at( "message" ).is_string() );
      CHECK( parsed.as_object().at( "message" ).as_string() == "ok" );
      REQUIRE( parsed.as_object().contains( "parameter" ) );
      REQUIRE( parsed.as_object().at( "parameter" ).is_string() );
      CHECK( parsed.as_object().at( "parameter" ).as_string() == "blah" );
    }

    AND_WHEN( "Making get request to data path with another path parameter" )
    {
      const auto [ct, resp] = ptest::response( "/data/somethingelse" );
      CHECK( ct == "application/json" );
      REQUIRE_FALSE( resp.empty() );

      auto ec = boost::system::error_code{};
      auto parsed = boost::json::parse( resp, ec );
      REQUIRE_FALSE( ec );
      REQUIRE( parsed.is_object() );
      REQUIRE( parsed.as_object().contains( "status" ) );
      REQUIRE( parsed.as_object().at( "status" ).is_number() );
      CHECK( parsed.as_object().at( "status" ).as_int64() == 200 );
      REQUIRE( parsed.as_object().contains( "message" ) );
      REQUIRE( parsed.as_object().at( "message" ).is_string() );
      CHECK( parsed.as_object().at( "message" ).as_string() == "ok" );
      REQUIRE( parsed.as_object().contains( "parameter" ) );
      REQUIRE( parsed.as_object().at( "parameter" ).is_string() );
      CHECK( parsed.as_object().at( "parameter" ).as_string() == "somethingelse" );
    }

    AND_WHEN( "Making get request to data path without path parameter" )
    {
      const auto [ct, resp] = ptest::response( "/data/" );
      CHECK( ct.starts_with( "application/json" ) );
      REQUIRE_FALSE( resp.empty() );

      auto ec = boost::system::error_code{};
      auto parsed = boost::json::parse( resp, ec );
      REQUIRE_FALSE( ec );
      REQUIRE( parsed.is_object() );
      REQUIRE( parsed.as_object().contains( "code" ) );
      REQUIRE( parsed.as_object().at( "code" ).is_number() );
      CHECK( parsed.as_object().at( "code" ).as_int64() == 404 );
      REQUIRE( parsed.as_object().contains( "cause" ) );
      REQUIRE( parsed.as_object().at( "cause" ).is_string() );
      CHECK( parsed.as_object().at( "cause" ).as_string() == "Not Found" );
      CHECK_FALSE( parsed.as_object().contains( "parameter" ) );
    }

    AND_WHEN( "Making post request to input path" )
    {
      const auto json = boost::json::object{
        { "now", std::chrono::system_clock::now().time_since_epoch().count() },
        { "string", "value" },
        { "nested", boost::json::object{ { "integer", 1234 }, { "number", 1234.5678 } } },
        { "client", "nghttp2::asio::client" } };
      const auto [ct, resp] = ptest::response( "/input", boost::json::serialize( json ) );
      CHECK( ct == "application/json" );
      REQUIRE_FALSE( resp.empty() );

      auto ec = boost::system::error_code{};
      auto parsed = boost::json::parse( resp, ec );
      REQUIRE_FALSE( ec );
      REQUIRE( parsed.is_object() );

      auto& obj = parsed.as_object();
      REQUIRE( obj.contains( "received" ) );
      REQUIRE( obj.at( "received" ).is_int64() );
      CHECK( obj.at( "received" ).as_int64() > json.at( "now" ).as_int64() );
      REQUIRE( obj.contains( "server" ) );
      REQUIRE( obj.at( "server" ).is_string() );
      REQUIRE( obj.contains( "payload" ) );
      REQUIRE( obj.at( "payload" ).is_object() );
      CHECK( obj.at( "payload" ).as_object() == json );
    }

    AND_WHEN( "Making request in a loop" )
    {
      for ( auto i = 0; i < 1024; i++ )
      {
        INFO( std::format( "Request {}", i ) );
        const auto json = boost::json::object{
          { "now", std::chrono::system_clock::now().time_since_epoch().count() },
          { "string", "value" },
          { "nested", boost::json::object{ { "integer", 1234 }, { "number", 1234.5678 } } },
          { "client", "nghttp2::asio::client" } };
        const auto [ct, resp] = ptest::response( "/input", boost::json::serialize( json ) );
        CHECK( ct == "application/json" );
        REQUIRE_FALSE( resp.empty() );

        auto ec = boost::system::error_code{};
        auto parsed = boost::json::parse( resp, ec );
        REQUIRE_FALSE( ec );
        REQUIRE( parsed.is_object() );

        auto& obj = parsed.as_object();
        REQUIRE( obj.contains( "received" ) );
        REQUIRE( obj.at( "received" ).is_int64() );
        CHECK( obj.at( "received" ).as_int64() > json.at( "now" ).as_int64() );
        REQUIRE( obj.contains( "server" ) );
        REQUIRE( obj.at( "server" ).is_string() );
        REQUIRE( obj.contains( "payload" ) );
        REQUIRE( obj.at( "payload" ).is_object() );
        CHECK( obj.at( "payload" ).as_object() == json );
      }
    }

    AND_WHEN( "Making requests in parallel" )
    {
      constexpr auto total = 1024;
      using R = std::tuple<std::string, std::string>;
      auto vec = std::vector<std::future<R>>{};
      vec.reserve( total );

      const auto json = boost::json::object{
        { "now", std::chrono::system_clock::now().time_since_epoch().count() },
        { "string", "value" },
        { "nested", boost::json::object{ { "integer", 1234 }, { "number", 1234.5678 } } },
        { "client", "nghttp2::asio::client" } };

      for ( auto i = 0; i < total; i++ )
      {
        vec.push_back( std::async( std::launch::async,
          [json]() { return ptest::response( "/input", boost::json::serialize( json ) ); } ) );
      }

      for ( auto& fut : vec )
      {
        const auto [ct, resp] = fut.get();
        CHECK( ct == "application/json" );
        REQUIRE_FALSE( resp.empty() );

        auto ec = boost::system::error_code{};
        auto parsed = boost::json::parse( resp, ec );
        REQUIRE_FALSE( ec );
        REQUIRE( parsed.is_object() );

        auto& obj = parsed.as_object();
        REQUIRE( obj.contains( "received" ) );
        REQUIRE( obj.at( "received" ).is_int64() );
        CHECK( obj.at( "received" ).as_int64() > json.at( "now" ).as_int64() );
        REQUIRE( obj.contains( "server" ) );
        REQUIRE( obj.at( "server" ).is_string() );
        REQUIRE( obj.contains( "payload" ) );
        REQUIRE( obj.at( "payload" ).is_object() );
        CHECK( obj.at( "payload" ).as_object() == json );
      }
    }

    AND_WHEN( "Making requests in parallel in a loop" )
    {
      using R = std::tuple<std::string, std::string>;
      constexpr auto total = 128;

      const auto json = boost::json::object{
        { "now", std::chrono::system_clock::now().time_since_epoch().count() },
        { "string", "value" },
        { "nested", boost::json::object{ { "integer", 1234 }, { "number", 1234.5678 } } },
        { "client", "nghttp2::asio::client" } };

      for ( auto i = 0; i < 16; i++ )
      {
        auto vec = std::vector<std::future<R>>{};
        vec.reserve( total );

        for ( auto j = 0; j < total; j++ )
        {
          vec.push_back( std::async( std::launch::async,
            [json]() { return ptest::response( "/input", boost::json::serialize( json ) ); } ) );
        }

        for ( auto& fut : vec )
        {
          const auto [ct, resp] = fut.get();
          CHECK( ct == "application/json" );
          REQUIRE_FALSE( resp.empty() );

          auto ec = boost::system::error_code{};
          auto parsed = boost::json::parse( resp, ec );
          REQUIRE_FALSE( ec );
          REQUIRE( parsed.is_object() );

          auto& obj = parsed.as_object();
          REQUIRE( obj.contains( "received" ) );
          REQUIRE( obj.at( "received" ).is_int64() );
          CHECK( obj.at( "received" ).as_int64() > json.at( "now" ).as_int64() );
          REQUIRE( obj.contains( "server" ) );
          REQUIRE( obj.at( "server" ).is_string() );
          REQUIRE( obj.contains( "payload" ) );
          REQUIRE( obj.at( "payload" ).is_object() );
          CHECK( obj.at( "payload" ).as_object() == json );
        }
      }
    }
  }
}

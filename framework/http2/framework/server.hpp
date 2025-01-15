//
// Created by Rakesh on 07/01/2025.
//

#pragma once

#include "configuration.hpp"
#include "router.hpp"
#include "stream.hpp"

#include <charconv>
#include <vector>

#include <boost/asio/thread_pool.hpp>
#include <boost/pfr/core_name.hpp>

namespace spt::http2::framework
{
  /**
   * HTTP2 server instance.  Configure parameters using the `Configuration` structure.  Add endpoint handlers
   * using the `addHandler` method.  Start the server after setting up all the handlers.
   *
   * Keep the calling process alive using some standard strategy.
   * \code{.cpp}
   * #include <http2/framework/server.hpp>
   *
   * struct Response
   * {
   *   Response( const nghttp2::asio_http2::header_map& headers )
   *   {
   *     // Fetch any necessary request header values.
   *   }
   *
   *   void set( std::span<const std::string> methods, std::span<const std::string> origins )
   *   {
   *     // Set response headers as appropriate using the desired methods to specify CORS headers as appropriate.
   *   }
   * };
   *
   * auto conf = spt::http2::framework::Configuration{};
   * conf.port = 8080;
   * conf.origins = std::vector{ "https://dashboard.sptci.com", "https://admin.sptci.com", "https://app.sptci.com" };
   * auto server = spt::http2::framework::Server<Response>{ conf };
   *
   * // Add handlers similar to:
   * server.addHandler( "GET", "/some/path/{param}", []( const spt::http2::framework::RoutingRequest& rr, const auto& params )
   * {
   *   auto resp = Response( rr.req.header );
   *   resp.headers.emplace( "content-type", "application/json" );
   *   resp.body = boost::json::serialize( boost::json::object{
   *     { "code", 200 },
   *     { "cause", "ok" },
   *     { "parameter", params.at( "param" ) }
   *   } );
   *   return resp;
   * } );
   *
   * server.start();
   *
   * // Run until signal to stop is received
   * auto ioc = boost::asio::io_context{};
   * boost::asio::signal_set signals( ioc, SIGINT, SIGTERM );
   * signals.async_wait( [&server](auto const&, int) { server.stop(); } );
   * ioc.run();
   * \endcode
   * @tparam Resp The response type that will be generated from the endpoint handlers.
   */
  template <Response Resp>
  struct Server
  {
    /**
     * Create a server instance using specified configuration.  Server implementation
     * expects only text data (JSON, YAML, ...) being submitted to endpoints.
     * @param config The server configuration object.
     */
    explicit Server( const Configuration& config ) : configuration{ config }, pool{ config.numberOfWorkerThreads } { init(); }

    /// A call back function used to scan any data submitted to the service.  Implement desired logic such
    /// as scanning for tags, scripts, invalid characters ... as appropriate.
    using Scanner = std::function<bool( std::string_view )>;

    /**
     * Create a server instance using specified configuration and input scanner function.  Scanner
     * function should _validate_ the input data and either _approve_ or _reject_ the content. Server implementation
     * expects only text data (JSON, YAML, ...) being submitted to endpoints.  Server will send a `400` response
     * if the function returns `false`.
     * @param config The server configuration object.
     * @param scanner Function to invoke when client sends payload in request (assume text data).  If function
     *   returns `false`, a `413` response is sent to client.
     */
    explicit Server( const Configuration& config, Scanner&& scanner ) :
        configuration{ config }, pool{ config.numberOfWorkerThreads }, scanner{ std::move( scanner ) }
    {
      init();
    }

    ~Server() { stop(); }

    /**
     *
     * @param method The HTTP method/verb for which the handler is being registered.
     * @param path The HTTP resource path for which the handler is being registered.
     * @param handler The handler function to which requests will be routed.
     * @param ref Optional reference to associate with the path when outputting the YAML for all handlers.
     */
    void addHandler( std::string_view method, std::string_view path, typename Router<Resp>::Handler&& handler, std::string_view ref = {} )
    {
      router.add( method, path, std::move( handler ), ref );
    }

    /**
     * Start the server.  Invoke this after setting up the endpoint handlers.
     */
    void start()
    {
#ifdef HAS_NANO_LOG
      LOG_INFO << "Starting server on " << configuration.host << ":" << configuration.port;
#if BOOST_VERSION > 108600
      auto obj = boost::json::object{};
      boost::pfr::for_each_field_with_name( configuration, [&obj](std::string_view name, const auto& value)
      {
        obj.emplace( name, std::format( "{}", value ) );
      } );
      LOG_INFO << boost::json::serialize( obj );
#endif
#endif
      boost::system::error_code ec;
      if ( server.listen_and_serve( ec, configuration.host, std::to_string( configuration.port ), true ) )
      {
#ifdef HAS_NANO_LOG
        LOG_CRIT << "error: " << ec.message();
#endif
        throw std::runtime_error( ec.message() );
      }
    }

    /**
     * Stop the server.  Stops the worker pool and server and joins on all outstanding threads.
     */
    void stop()
    {
#ifdef HAS_NANO_LOG
      LOG_INFO << "Stopping server on " << configuration.host << ":" << configuration.port;
#endif
      server.stop();
      pool.stop();
      server.join();
      pool.join();
    }

  private:
    void init()
    {
      server.backlog( configuration.backlog );

      server.handle( "/", [this](const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res)
      {
        handle( req, res );
      });
    }

    void handlePayload( const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res )
    {
      auto body = std::make_shared<std::string>();

      const auto error = []( uint16_t code, std::string_view msg, const nghttp2::asio_http2::server::response& res )
      {
        res.write_head( code, { { "content-type", { "application/json; charset=utf-8", false } } } );
        res.end( boost::json::serialize( boost::json::object{ { "code", code }, { "cause", msg } } ) );
      };

      if ( !router.canRoute( req.method(), req.uri().path ) )  return error( 404, "Not Found", res );

      auto iter = req.header().find( "content-length"s );
      if ( iter == req.header().end() ) iter = req.header().find( "Content-Length"s );
      if ( iter == req.header().end() )
      {
        body->reserve( 2048 );
      }
      else
      {
        uint32_t length{};
        auto [ptr, ec] { std::from_chars( iter->second.value.data(), iter->second.value.data() + iter->second.value.size(), length ) };
        if ( ec == std::errc() )
        {
          if ( length > configuration.maxPayloadSize ) return error( 413, "Payload Too Large", res );
          body->reserve( length );
        }
        else
        {
#ifdef HAS_NANO_LOG
          LOG_WARN << "Invalid content-length: " << iter->second.value;
#endif
          body->reserve( 2048 );
        }
      }

      req.on_data([body, &req, &res, &error, this](const uint8_t* chars, std::size_t size)
      {
        if ( size )
        {
          body->append( reinterpret_cast<const char*>( chars ), size );
          return;
        }

        if ( body->size() > configuration.maxPayloadSize ) return error( 413, "Payload Too Large", res );

        if ( scanner && !scanner( *body ) ) return error( 400, "Prohibited input", res );

        auto stream = std::make_shared<Stream<Resp>>( req, res, router, body );
        res.on_close( [stream]([[maybe_unused]] uint32_t errorCode)
        {
#ifdef HAS_NANO_LOG
          if ( errorCode ) LOG_INFO << "Client closed connection with error " << errorCode;
#endif
          stream->close( true );
        } );

        boost::asio::post( pool, [stream, this]{ stream->process( pool, configuration ); } );
      });
    }

    void handle( const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res )
    {
      if ( req.method() == "OPTIONS" ) return cors( req, res, configuration );

      if ( req.method() == "POST" || req.method() == "PUT" || req.method() == "PATCH" )
      {
        return handlePayload( req, res );
      }

      auto stream = std::make_shared<Stream<Resp>>( req, res, router );
      res.on_close( [stream]([[maybe_unused]] uint32_t errorCode)
      {
#ifdef HAS_NANO_LOG
        if ( errorCode ) LOG_INFO << "Client closed connection with error " << errorCode;
#endif
        stream->close( true );
      } );

#ifdef HAS_NANO_LOG
      LOG_DEBUG << "Enqueueing " << req.method() << " request for " << req.uri().path;
#endif
      boost::asio::post( pool, [stream, this]{ stream->process( pool, configuration ); } );
    }

    Configuration configuration;
    boost::asio::thread_pool pool;
    nghttp2::asio_http2::server::http2 server;
    Router<Resp> router;
    Scanner scanner;
  };
}
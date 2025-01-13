//
// Created by Rakesh on 06/01/2025.
//

#pragma once

#include "common.hpp"
#include "request.hpp"
#include "response.hpp"

#include <router/router.hpp>

namespace spt::http2::framework
{
  using std::operator ""s;

  /**
   * Simple struct used to wrap a request and any payload the client sent.
   */
  struct RoutingRequest
  {
    /// Request wrapper with copies of necessary data to allow use even if client has disconnected while server
    /// is generating the response.
    const Request& req;

    /// The server configuration object.  Grants handlers access to useful configured values such as default
    /// CORS methods and origins supported by server.
    const Configuration& configuration;

    /// Any payload sent by client (usually part of POST/PUT/PATCH).
    std::string_view body{};
  };

  /**
   * Wrapper around the `spt::http::router::HttpRouter` with a few standard error handlers.
   * @tparam Resp The response type the endpoint handlers will generate.
   */
  template <Response Resp>
  struct Router
  {
    using Params = boost::container::flat_map<std::string_view, std::string_view>;
    using Handler = std::function<Resp( const RoutingRequest&, Params&& )>;
    Router& add( std::string_view method, std::string_view path, Handler&& handler, std::string_view ref = {} )
    {
      router.add( method, path, std::forward<Handler>( handler ), ref );
      return *this;
    }

    [[nodiscard]] std::optional<Resp> route( std::string_view method, std::string_view path, const RoutingRequest& request ) const
    {
      return router.route( method, path, request );
    }

    [[nodiscard]] bool canRoute( std::string_view method, std::string_view path ) const
    {
      return router.canRoute( method, path );
    }

    [[nodiscard]] std::string yaml() const { return router.yaml(); }

    Router() = default;
    ~Router() = default;
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

  private:
    static Resp error404( const RoutingRequest& rr, Params&& )
    {
      return error<Resp>( 404, methods, rr.req.header, Configuration{} );
    }

    static Resp error405( const RoutingRequest& rr, Params&& )
    {
      return error<Resp>( 405, methods, rr.req.header, Configuration{} );
    }

    static Resp error500( const RoutingRequest& rr, Params&& )
    {
      return error<Resp>( 500, methods, rr.req.header, Configuration{} );
    }

    inline static const auto methods = std::array{ "DELETE"s, "GET"s, "OPTIONS"s, "POST"s, "PUT"s };
    http::router::HttpRouter<const RoutingRequest&, Resp> router{ &Router::error404, &Router::error405, &Router::error500 };
  };
}

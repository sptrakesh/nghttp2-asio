#include <iostream>

#include <boost/asio/signal_set.hpp>
#include <nghttp2/asio_http2_server.h>

using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::server;

int main()
{
  boost::system::error_code ec;
  http2 server;

  server.handle("/", [](const request&, const response& res)
  {
    res.write_head(200);
    res.end("hello, world\n");
  });

  std::cout << "Starting HTTP/2 server on localhost:3000\n";
  std::cout << "Test with curl --http2-prior-knowledge http://localhost:3000\n";
  if (server.listen_and_serve(ec, "localhost", "3000", true))
  {
    std::cerr << "error: " << ec.message() << std::endl;
  }

  boost::asio::signal_set signals( *server.io_contexts().front(), SIGINT, SIGTERM );
  signals.async_wait( [&server](auto const&, int ) { server.stop(); server.join(); } );

  server.io_contexts().front()->run();
}

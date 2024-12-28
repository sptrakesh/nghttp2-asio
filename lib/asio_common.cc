/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "asio_common.h"

#include <fcntl.h>
#ifdef _WIN32
#  include <io.h>
#endif

#include "util.h"
#include "template.h"
#include "http2.h"

#include <boost/url/parse.hpp>

namespace nghttp2 {
namespace asio_http2 {

class nghttp2_category_impl : public boost::system::error_category {
public:
  const char *name() const noexcept { return "nghttp2"; }
  std::string message(int ev) const { return nghttp2_strerror(ev); }
};

const boost::system::error_category &nghttp2_category() noexcept {
  static nghttp2_category_impl cat;
  return cat;
}

boost::system::error_code make_error_code(nghttp2_error ev) {
  return boost::system::error_code(ev, nghttp2_category());
}

class nghttp2_asio_category_impl : public boost::system::error_category {
public:
  const char *name() const noexcept { return "nghttp2_asio"; }
  std::string message(int ev) const {
    auto er = static_cast<nghttp2_asio_error>( ev );
    switch (er) {
    case nghttp2_asio_error::NGHTTP2_ASIO_ERR_NO_ERROR:
      return "no error";
    case nghttp2_asio_error::NGHTTP2_ASIO_ERR_TLS_NO_APP_PROTO_NEGOTIATED:
      return "tls: no application protocol negotiated";
    default:
      return "unknown";
    }
  }
};

const boost::system::error_category &nghttp2_asio_category() noexcept {
  static nghttp2_asio_category_impl cat;
  return cat;
}

boost::system::error_code make_error_code(nghttp2_asio_error ev) {
  return boost::system::error_code(static_cast<int>(ev),
                                   nghttp2_asio_category());
}

class nghttp2_error_code_category_impl : public boost::system::error_category {
public:
  const char *name() const noexcept { return "nghttp2_error_code"; }
  std::string message(int ev) const noexcept { return nghttp2_http2_strerror(ev); }
};

const boost::system::error_category &nghttp2_error_code_category() noexcept {
  static nghttp2_error_code_category_impl cat;
  return cat;
}

boost::system::error_code make_error_code(nghttp2_error_code ev){
  return boost::system::error_code(static_cast<int>(ev),
                                     nghttp2_error_code_category());
}

generator_cb string_generator(std::string data) {
  auto strio = std::make_shared<std::pair<std::string, size_t>>(std::move(data),
                                                                data.size());
  return [strio](uint8_t *buf, size_t len, uint32_t *data_flags) {
    auto &data = strio->first;
    auto &left = strio->second;
    auto n = std::min<size_t>(len, left);
    std::copy_n(data.c_str() + data.size() - left, n, buf);
    left -= n;
    if (left == 0) {
      *data_flags |= NGHTTP2_DATA_FLAG_EOF;
    }
    return n;
  };
}

generator_cb deferred_generator() {
  return [](uint8_t *buf, size_t len, uint32_t *data_flags) {
    return NGHTTP2_ERR_DEFERRED;
  };
}

template <typename F, typename... T>
std::shared_ptr<Defer<F, T...>> defer_shared(F &&f, T &&...t) {
  return std::make_shared<Defer<F, T...>>(std::forward<F>(f),
                                          std::forward<T>(t)...);
}

#ifdef _WIN32
#  define FD_MAGIC(f) _##f
#else
#  define FD_MAGIC(f) f
#endif

generator_cb file_generator(const std::string &path) {
  auto fd = FD_MAGIC(open)(path.c_str(), FD_MAGIC(O_RDONLY));
  if (fd == -1) {
    return generator_cb();
  }

  return file_generator_from_fd(fd);
}

generator_cb file_generator_from_fd(int fd) {
  auto d = defer_shared(FD_MAGIC(close), fd);

  return [fd, d](uint8_t *buf, size_t len,
                 uint32_t *data_flags) -> generator_cb::result_type {
    ssize_t n;
    while ((n = FD_MAGIC(read)(fd, buf, len)) == -1 && errno == EINTR)
      ;

    if (n == -1) {
      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }

    if (n == 0) {
      *data_flags |= NGHTTP2_DATA_FLAG_EOF;
    }

    return n;
  };
}

bool check_path(const std::string &path) { return util::check_path(path); }

std::string percent_decode(const std::string &s) {
  return util::percent_decode(std::begin(s), std::end(s));
}

std::string http_date(int64_t t) { return util::http_date(t); }

boost::system::error_code host_service_from_uri(boost::system::error_code &ec,
                                                std::string &scheme,
                                                std::string &host,
                                                std::string &service,
                                                const std::string &uri) {
  ec.clear();

  auto result = boost::urls::parse_uri(uri);
  if (result.has_error()) {
    ec = make_error_code(boost::system::errc::invalid_argument);
    return ec;
  }

  if (!result.value().has_scheme() || !result.value().has_authority()) {
    ec = make_error_code(boost::system::errc::invalid_argument);
    return ec;
  }

  scheme = result.value().scheme();
  host = result.value().host();

  if (result.value().port_number() > 0) {
    service = result.value().port();
  } else {
    service = scheme;
  }

  return ec;
}

bool tls_h2_negotiated(ssl_socket &socket) {
  auto ssl = socket.native_handle();

  const unsigned char *next_proto = nullptr;
  unsigned int next_proto_len = 0;

#ifndef OPENSSL_NO_NEXTPROTONEG
  SSL_get0_next_proto_negotiated(ssl, &next_proto, &next_proto_len);
#endif // !OPENSSL_NO_NEXTPROTONEG
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
  if (next_proto == nullptr) {
    SSL_get0_alpn_selected(ssl, &next_proto, &next_proto_len);
  }
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L

  if (next_proto == nullptr) {
    return false;
  }

  return util::check_h2_is_selected(StringRef{next_proto, next_proto_len});
}

} // namespace asio_http2
} // namespace nghttp2

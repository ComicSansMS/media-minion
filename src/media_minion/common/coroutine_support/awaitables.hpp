#ifndef MEDIA_MINION_INCLUDE_GUARD_COMMON_COROUTINE_SUPPORT_AWAITABLES_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_COMMON_COROUTINE_SUPPORT_AWAITABLES_HPP_

#ifndef __cpp_coroutines
#   error Coroutines are not available :(
#endif

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/system/error_code.hpp>

#include <experimental/coroutine>

#include <string>
#include <string_view>
#include <tuple>

namespace media_minion::coroutine {

template<typename P>
struct IoContextProvider {
    boost::asio::io_context* m_io_ctx;
    IoContextProvider(boost::asio::io_context& io)
        :m_io_ctx(&io)
    {}

    bool await_ready() { return false; }

    bool await_suspend(std::experimental::coroutine_handle<P> h) {
        h.promise().io_ctx = m_io_ctx;
        return false;
    }

    void await_resume() {}
};


template<typename T>
struct ErrorResult {
    boost::system::error_code error;
    T value;

    ErrorResult() = default;

    ErrorResult(T const& v)
        :error(), value(v)
    {}

    ErrorResult(T&& v)
        :error(), value(std::move(v))
    {}
};

template<>
struct ErrorResult<void> {
    boost::system::error_code error;

    ErrorResult() = default;
};


template<typename R>
class BaseAwaitable {
protected:
    ErrorResult<R> m_result;
public:
    bool await_ready() { return false; }

    auto await_resume() {
        return std::move(m_result);
    }
};


class AsyncResolve : public BaseAwaitable<boost::asio::ip::tcp::resolver::results_type> {
private:
    boost::asio::ip::tcp::resolver& m_resolver;
    std::string_view m_host;
    std::string_view m_service;
public:
    AsyncResolve(boost::asio::ip::tcp::resolver& resolver, std::string_view host, std::string_view service)
        :m_resolver(resolver), m_host(host), m_service(service)
    {}

    void await_suspend(std::experimental::coroutine_handle<> h) {
        m_resolver.async_resolve(m_host, m_service,
            [this, h](boost::system::error_code const& ec, boost::asio::ip::tcp::resolver::results_type results) mutable {
                m_result.error = ec;
                m_result.value = std::move(results);
                h.resume();
            });
    }
};


class AsyncConnect : public BaseAwaitable<boost::asio::ip::tcp::endpoint> {
private:
    boost::asio::ip::tcp::socket& m_socket;
    boost::asio::ip::tcp::resolver::results_type const& m_resolverResults;

public:
    AsyncConnect(boost::asio::ip::tcp::socket& socket, boost::asio::ip::tcp::resolver::results_type const& res_res)
        :m_socket(socket), m_resolverResults(res_res)
    {}

    void await_suspend(std::experimental::coroutine_handle<> h) {
        boost::asio::async_connect(m_socket, m_resolverResults,
            [this, h](boost::system::error_code const& ec, boost::asio::ip::tcp::endpoint const& endpoint) mutable {
                m_result.error = ec;
                m_result.value = endpoint;
                h.resume();
            });
    }
};

class AsyncHandshake : public BaseAwaitable<void> {
private:
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket>& m_websocket;
    boost::beast::string_view m_hostname;
    boost::beast::string_view m_target;
public:
    AsyncHandshake(boost::beast::websocket::stream<boost::asio::ip::tcp::socket>& websocket,
                   boost::beast::string_view hostname, boost::beast::string_view target)
        :m_websocket(websocket), m_hostname(hostname), m_target(target)
    {}

    void await_suspend(std::experimental::coroutine_handle<> h) {
        m_websocket.async_handshake(m_hostname, m_target, [this, h](boost::system::error_code const& ec) mutable {
                m_result.error = ec;
                h.resume();
            });
    }
};

template<typename DynamicBuffer>
class AsyncRead : public BaseAwaitable<std::size_t> {
private:
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket>& m_websocket;
    DynamicBuffer& m_buffer;
public:
    AsyncRead(boost::beast::websocket::stream<boost::asio::ip::tcp::socket>& websocket,
              DynamicBuffer& buffer)
        :m_websocket(websocket), m_buffer(buffer)
    {}

    void await_suspend(std::experimental::coroutine_handle<> h) {
        m_websocket.async_read(m_buffer, [this, h](boost::system::error_code const& ec, std::size_t bytes) mutable {
                m_result.error = ec;
                m_result.value = bytes;
                h.resume();
            });
    }
};


}

#endif

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <iostream>
#include <regex>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <utility>
#include <vector>
#include <functional>

using namespace boost;
using namespace boost::beast;

class HTTPServer {
public:

    enum class Method { GET, POST, PUT, DELETE };

    struct ParsedURL {
        ParsedURL() = default;
        explicit ParsedURL(const std::string& target);
        std::string path;
        std::string extension;
        std::string query;
        std::vector<std::string> path_params;
        std::vector<std::pair<std::string, std::string>> query_params;
    };

    class Session; friend class Session;

    class Route {
        friend class HTTPServer;
    public:
        using Handler = std::function<void(HTTPServer::Session&)>;
        explicit Route(Method method, std::string pattern, Handler handler);
        [[nodiscard]] std::pair<bool, std::smatch> match(const std::string& target) const;
    private:
        Method _method;
        std::string _pattern;
        std::regex _regex;
        std::function<void(HTTPServer::Session&)> _handler;
    };

    class Session : public std::enable_shared_from_this<Session> {

    public:
        explicit Session(HTTPServer& server, asio::ip::tcp::socket socket, asio::ssl::context& ssl);
        void start();
        [[nodiscard]] const ParsedURL& url() const;
        [[nodiscard]] const http::request<http::dynamic_body>& request() const;

//    private:
        void _read();
        bool _file_exists(const std::string& path);

        template<typename BodyType>
        void _send_response(http::response<BodyType>&& response) {
            http::async_write(_stream, response, [self = shared_from_this()]
                (boost::beast::error_code ec, std::size_t size) {

                if (ec && ec != beast::http::error::short_read) {
                    std::cerr << "HTTPServer: write failed: " << ec.what() << std::endl;
                    return;
                }

                boost::ignore_unused(size);
                self->_stream.shutdown();
            });
        }

        HTTPServer& _server;
        asio::ssl::stream<asio::ip::tcp::socket> _stream;
        beast::flat_buffer _read_buf{8192};
        http::request<http::dynamic_body> _request;
        ParsedURL _url;
    };

public:
    explicit HTTPServer(asio::io_service& io, unsigned short port, asio::ssl::context& ssl);
    void serve_directory(const std::string& path);
    void add_route(Method method, const std::string& pattern, const Route::Handler& handler);

    /* TODO: implement sth. like this:
    void on_request(std::function<void(Session&)> handler);
    void on_error(std::function<void(Session&, const boost::beast::error_code&)> handler);
    */

    static std::string mime_type(const std::string& extension);

private:
    void _accept();

    asio::ip::tcp::acceptor _acceptor;
    asio::ip::tcp::socket _socket;
    std::vector<Route> _routes;
    asio::ssl::context& _ssl;
    std::optional<std::string> _static_root = std::nullopt;
};

#endif

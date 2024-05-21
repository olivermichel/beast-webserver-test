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

    struct ParsedURL {

        ParsedURL() = default;

        explicit ParsedURL(const std::string& target) {

            // split into path and query part:

            auto pos = target.find('?');

            if (pos != std::string::npos) {
                query = target.substr(pos + 1);
                path = target.substr(0, pos);
            } else {
                path = target;
            }

            // strip trailing / from path:

            if (path.size() > 1 && path.back() == '/') {
                path.pop_back();
            }

            // split query by &:

            std::string::size_type start = 0;

            while (start < query.size()) {

                auto end = query.find('&', start);

                if (end == std::string::npos) {
                    end = query.size();
                }

                auto query_string = query.substr(start, end - start);

                //split param by =
                pos = query_string.find('=');

                if (pos != std::string::npos) {
                    std::string key = query_string.substr(0, pos);
                    std::string value = query_string.substr(pos + 1);
                    query_params.emplace_back(key, value);
                }

                start = end + 1;
            }
        }

        std::string path;
        std::string query;
        std::vector<std::string> path_params;
        std::vector<std::pair<std::string, std::string>> query_params;
    };

    class Session;
    friend class Session;

    class Route {

        friend class HTTPServer;

    public:

        using Handler = std::function<void(HTTPServer::Session&)>;

        explicit Route(std::string pattern, Handler handler)
                : _pattern(std::move(pattern)),
                  _regex(_pattern),
                  _handler(std::move(handler)) { }

        [[nodiscard]] std::pair<bool, std::smatch> match(const std::string& target) const {
            std::smatch match;
            auto result = std::regex_match(target, match, _regex);
            return {result, match};
        }

    private:
        std::string _pattern;
        std::regex _regex;
        std::function<void(HTTPServer::Session&)> _handler;
    };

    class Session : public std::enable_shared_from_this<Session> {

    public:

        explicit Session(HTTPServer& server, asio::ip::tcp::socket socket, asio::ssl::context& ssl)
            : _server{server},
              _stream{std::move(socket), ssl} { }

        void start() {
            _stream.async_handshake(asio::ssl::stream_base::server,
                [this, self = shared_from_this()](boost::beast::error_code ec) {

                if (!ec) {
                    self->_read();
                }
            });
        }

        [[nodiscard]] const ParsedURL& url() const {
            return _url;
        }

        [[nodiscard]] const http::request<http::dynamic_body>& request() const {
            return _request;
        }

        [[nodiscard]] http::response<http::dynamic_body>& response() {
            return _response;
        }

    private:

        void _read() {

            http::async_read(_stream, _read_buf, _request,
                [this, self = this->shared_from_this()]
                (boost::system::error_code ec, std::size_t len) {

                 bool found_route = false;
                 _url = ParsedURL{_request.target()};
                 boost::ignore_unused(len);

                 if (!ec) {

                     for (const auto& route : _server._routes) {

                         auto [success, match] = route.match(_url.path);

                         if (success) {
                             found_route = true;

                             for (auto i = 1; i < match.size(); ++i)
                                 _url.path_params.emplace_back(match[i]);

                             route._handler(*this);
                         }
                     }

                     if (!found_route) {
                         _response.result(http::status::not_found);
                     }

                     _response.version(_request.version());
                     _response.keep_alive(false);

                     http::async_write(_stream, _response,
                        [self](boost::beast::error_code ec, std::size_t) {
                        self->_stream.shutdown();
                     });
                 }
            });
        }

        HTTPServer& _server;
        asio::ssl::stream<asio::ip::tcp::socket> _stream;
        beast::flat_buffer _read_buf{8192};
        http::request<http::dynamic_body> _request;
        http::response<http::dynamic_body> _response;
        ParsedURL _url;
    };

public:
    explicit HTTPServer(asio::io_service& io, unsigned short port, asio::ssl::context& ssl)
            : _acceptor{io, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), port}},
              _socket{io},
              _ssl{ssl} {

        _accept();
    }

    void add_route(const std::string& pattern, const Route::Handler& handler) {
        _routes.emplace_back(pattern, handler);
    }

private:
    void _accept() {
        _acceptor.async_accept(_socket, [this](std::error_code ec) {
            std::make_shared<Session>(*this, std::move(_socket), _ssl)->start();
            _accept();
        });
    }

    asio::ip::tcp::acceptor _acceptor;
    asio::ip::tcp::socket _socket;
    std::vector<Route> _routes;
    asio::ssl::context& _ssl;
};

#endif

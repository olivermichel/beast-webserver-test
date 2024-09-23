
#include "http_server.h"

#include <filesystem>

HTTPServer::ParsedURL::ParsedURL(const std::string& target) {

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

    // get extension from path:
    if (path.find('.') != std::string::npos) {
        extension = path.substr(path.find_last_of('.') + 1);
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

HTTPServer::Route::Route(Method method, std::string pattern, Handler handler)
    : _method(method),
      _pattern(std::move(pattern)),
      _regex(_pattern),
      _handler(std::move(handler)) {

    if (_method != Method::GET) {
        throw std::domain_error("Route: unsupported method");
    }
}

std::pair<bool, std::smatch> HTTPServer::Route::match(const std::string& target) const {
    std::smatch match;
    auto result = std::regex_match(target, match, _regex);
    return {result, match};
}

HTTPServer::Session::Session(HTTPServer& server, asio::ip::tcp::socket socket,
                             asio::ssl::context& ssl)
    : _server{server},
      _stream{std::move(socket), ssl} { }

void HTTPServer::Session::start() {
    _stream.async_handshake(asio::ssl::stream_base::server,
        [this, self = shared_from_this()](boost::beast::error_code ec) {

        if (!ec) {
            self->_read();
        }
    });
}

const HTTPServer::ParsedURL& HTTPServer::Session::url() const {
    return _url;
}

const http::request<http::dynamic_body>& HTTPServer::Session::request() const {
    return _request;
}

void HTTPServer::Session::_read() {

    http::async_read(_stream, _read_buf, _request,
        [this, self = this->shared_from_this()]
        (boost::system::error_code ec, std::size_t len) {

        if (ec) {
            std::cout << ec.what() << std::endl;
            // handle error
            return;
        }

        _url = ParsedURL{_request.target()};
        boost::ignore_unused(len);

        if (_server._static_root && _file_exists(_url.path)) { // serve a static file:

            beast::error_code ec1;
            beast::http::file_body::value_type body;

            body.open(("." + _url.path).c_str(), beast::file_mode::scan, ec);

            std::cout << _url.extension << std::endl;

            if (ec) {
                std::cout << "open file failed: " << ec1.what() << std::endl;
            }

            auto size = body.size();

            http::response<http::file_body> response{
                std::piecewise_construct,
                std::make_tuple(std::move(body)),
                std::make_tuple(http::status::ok, _request.version())};

            response.set(http::field::server, BOOST_BEAST_VERSION_STRING);

            if (!_url.extension.empty()) {
                response.set(http::field::content_type, HTTPServer::mime_type(_url.extension));
            } else {
                response.set(http::field::content_type, "application/octet-stream");
            }

            response.content_length(size);

            http::write(_stream, response, ec);

            if (ec1) {
                std::cout << "read file failed: " << ec1.what() << std::endl;
            }

            return;
        } else {
            std::cout << "file does not exist " << _url.path << std::endl;
        }







        for (const auto& route : _server._routes) {
            auto [success, match] = route.match(_url.path);
            if (success) {
                // found_route = true;
                for (auto i = 1; i < match.size(); ++i)
                    _url.path_params.emplace_back(match[i]);
                route._handler(*this);
            }
        }
        /*
        if (!found_route) {
            _response.result(http::status::not_found);
        }

        _response.version(_request.version());
        _response.keep_alive(false);

        http::async_write(_stream, _response,
            [self](boost::beast::error_code ec, std::size_t) {
            self->_stream.shutdown();
        });
        */

    });
}

bool HTTPServer::Session::_file_exists(const std::string& path) {
    return std::filesystem::exists(*(_server._static_root) + path);
}

HTTPServer::HTTPServer(asio::io_service& io, unsigned short port, asio::ssl::context& ssl)
    : _acceptor{io, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), port}},
      _socket{io},
      _ssl{ssl} {

    _accept();

    std::cout << "HTTPServer: listening on port " << port << std::endl;
}

void HTTPServer::serve_directory(const std::string& path) {

    _static_root = path;
}

void HTTPServer::add_route(Method method, const std::string& pattern,
                           const Route::Handler& handler) {
    _routes.emplace_back(method, pattern, handler);
}

std::string HTTPServer::mime_type(const std::string& extension) {

    if (extension == "htm")  return "text/html";
    if (extension == "html") return "text/html";
    if (extension == "php")  return "text/html";
    if (extension == "css")  return "text/css";
    if (extension == "txt")  return "text/plain";
    if (extension == "js")   return "application/javascript";
    if (extension == "json") return "application/json";
    if (extension == "xml")  return "application/xml";
    if (extension == "swf")  return "application/x-shockwave-flash";
    if (extension == "flv")  return "video/x-flv";
    if (extension == "png")  return "image/png";
    if (extension == "jpe")  return "image/jpeg";
    if (extension == "jpeg") return "image/jpeg";
    if (extension == "jpg")  return "image/jpeg";
    if (extension == "gif")  return "image/gif";
    if (extension == "bmp")  return "image/bmp";
    if (extension == "ico")  return "image/vnd.microsoft.icon";
    if (extension == "tiff") return "image/tiff";
    if (extension == "tif")  return "image/tiff";
    if (extension == "svg")  return "image/svg+xml";
    if (extension == "svgz") return "image/svg+xml";

    return "application/octet-stream";
}

void HTTPServer::_accept() {
    _acceptor.async_accept(_socket, [this](std::error_code ec) {
        std::make_shared<Session>(*this, std::move(_socket), _ssl)->start();
        _accept();
    });
}

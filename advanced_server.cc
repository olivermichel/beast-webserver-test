
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

#include <iostream>
#include <filesystem>
#include <utility>

using namespace boost;
using namespace boost::beast;

/*
static std::string read_file(const std::string& file_name) {

    std::ifstream file{file_name};

    if (!file.is_open())
        throw std::runtime_error{"read_file: failed to open " + file_name};

    return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}
*/

asio::ssl::context initialize_ssl(const std::string& cert_file, const std::string& key_file) {

    asio::ssl::context ssl{asio::ssl::context::tlsv12};

    ssl.set_options(asio::ssl::context::default_workarounds |
                    asio::ssl::context::no_sslv2 |
                    asio::ssl::context::single_dh_use);

    boost::system::error_code ec;

    ec = ssl.use_certificate_chain_file(cert_file, ec);

    if (ec) {
        throw std::runtime_error{"use_certificate_chain_file: failed: " + ec.message()};
    }

    ec = ssl.use_private_key_file(key_file, boost::asio::ssl::context::file_format::pem, ec);

    if (ec) {
        throw std::runtime_error{"use_private_key_file: failed: " + ec.message()};
    }

    return ssl;
}

class HTTPServer {

public:

    class Target {

    public:

        explicit Target(const std::string& target) {
            _parseURL(target);
        }

        [[nodiscard]] const std::string& path() const {
            return _path;
        }

        [[nodiscard]] const std::string& fileExtension() const {
            return _fileExtension;
        }

        [[nodiscard]] const std::map<std::string, std::string>& params() const {
            return _params;
        }

    private:

        static std::string _decodeURL(const std::string& str) {

            std::string decoded;
            char ch;
            int i, ii;

            for (i = 0; i < str.length(); i++) {

                if (str[i] == '%') {

                    sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);

                    ch = static_cast<char>(ii);
                    decoded += ch;
                    i = i + 2;
                } else if (str[i] == '+') {
                    decoded += ' ';
                } else {
                    decoded += str[i];
                }
            }

            return decoded;
        }

        void _parseURL(const std::string &url) {

            std::map<std::string, std::string> params;
            std::string path, file_extension;

            // Find the position of the query part (`?`)
            std::string::size_type pos = url.find('?');
            std::string path_and_file = (pos == std::string::npos) ? url : url.substr(0, pos);
            std::string query = (pos == std::string::npos) ? "" : url.substr(pos + 1);

            // Extract file extension, if any, from the path
            std::string::size_type ext_pos = path_and_file.rfind('.');

            if (ext_pos != std::string::npos && ext_pos > path_and_file.rfind('/')) {
                _fileExtension = path_and_file.substr(ext_pos + 1);
            }

            _path = path_and_file;

            // Split the query string into key-value pairs
            std::istringstream query_stream(query);
            std::string pair;

            while (std::getline(query_stream, pair, '&')) {

                std::string::size_type equals_pos = pair.find('=');

                if (equals_pos != std::string::npos) {
                    std::string key = _decodeURL(pair.substr(0, equals_pos));
                    std::string value = _decodeURL(pair.substr(equals_pos + 1));
                    _params[key] = value;
                } else {
                    // Handle case where there's a key with no value
                    std::string key = _decodeURL(pair);
                    _params[key] = "";
                }
            }
        }

        std::string _path;
        std::string _fileExtension;
        std::map<std::string, std::string> _params;
    };

    explicit HTTPServer(asio::io_service& io, unsigned short port, asio::ssl::context& ssl)
        : _acceptor{io, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), port}},
          _socket{io},
          _ssl{ssl} {

        _accept();
    }

    void serveDirectory(const std::string& docRoot) {
        _docRoot = docRoot;
    }

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

        const http::request<http::dynamic_body>& request() const {
            return _request;
        }

       asio::ssl::stream<asio::ip::tcp::socket>& stream() {
            return _stream;
       }

    private:

        void _read() {

            http::async_read(_stream, _read_buf, _request,
                [this, self = this->shared_from_this()]
                (boost::system::error_code ec, std::size_t len) {

                _handleRequest();
            });
        }

        void _handleRequest() {

            Target target{std::string{_request.target()}};

            /*
            std::cout << "_handleRequest(): " << _request.method_string() << " "
                      << _request.target() << std::endl;
            */

            boost::beast::error_code ec;

            if (_server._docRoot) {

                auto filePath = _server._docRoot.value() + target.path();

                if (std::filesystem::exists(filePath)) {

                    if (std::filesystem::is_directory(filePath)) {

                        if (std::filesystem::exists(filePath + "/index.html")) {
                            filePath += "/index.html";
                        } else {
                            _returnNotFound();
                            return;
                        }
                    }

                    beast::http::file_body::value_type body;

                    body.open(filePath.c_str(), beast::file_mode::scan, ec);

                    if (ec) {
                        std::cerr << "failed opening " << filePath << ": " << ec.what() << std::endl;
                        return;
                    }

                    http::response<http::file_body> response{
                        std::piecewise_construct, std::make_tuple(std::move(body)),
                        std::make_tuple(http::status::ok, _request.version())};

                    response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    http::write(_stream, response, ec);

                    if (ec) {
                        std::cerr << ec.what() << std::endl;
                    }

                    return;
                }
            }

            if (_server._routes.find(target.path()) != _server._routes.end()) {

                _server._routes[target.path()](*this);
                return;
            }

            _returnNotFound();
        }

        void _returnNotFound() {

            http::response<http::string_body> response{http::status::not_found, _request.version()};
            response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            response.set(http::field::content_type, "text/html");
            response.keep_alive(_request.keep_alive());
            response.body() = "file not found";
            response.prepare_payload();
            http::write(_stream, response);
        }

        HTTPServer& _server;
        asio::ssl::stream<asio::ip::tcp::socket> _stream;
        beast::flat_buffer _read_buf;
        http::request<http::dynamic_body> _request;
    };

    void addRoute(const std::string& path, std::function<void (Session&)> handler) {

        _routes[path] = std::move(handler);
    }

    friend class HTTPServer;

private:

    void _accept() {
        _acceptor.async_accept(_socket, [this](std::error_code ec) {
            std::make_shared<Session>(*this, std::move(_socket), _ssl)->start();
            _accept();
        });
    }

    asio::ip::tcp::acceptor _acceptor;
    asio::ip::tcp::socket _socket;
    asio::ssl::context& _ssl;
    std::optional<std::string> _docRoot;
    std::unordered_map<std::string, std::function<void(Session&)>> _routes;
};

int main() {

    inja::Environment inja;
    asio::io_service io{};

    auto ssl = initialize_ssl("cert.pem", "key.pem");

    HTTPServer server{io, 8081, ssl};

    server.serveDirectory("doc_root");

    server.addRoute("/test", [](HTTPServer::Session& session) {
        http::response<http::string_body> response{http::status::ok, session.request().version()};
        response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        response.set(http::field::content_type, "text/html");
        response.keep_alive(session.request().keep_alive());
        response.body() = "hello test";
        response.prepare_payload();
        http::write(session.stream(), response);
    });

    io.run();

    return 0;
}

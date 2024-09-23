
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

using namespace boost;
using namespace boost::beast;

static std::string read_file(const std::string& file_name) {

    std::ifstream file{file_name};

    if (!file.is_open())
        throw std::runtime_error{"read_file: failed to open " + file_name};

    return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

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

    private:

        void _read() {

            http::async_read(_stream, _read_buf, _request,
                [this, self = this->shared_from_this()]
                (boost::system::error_code ec, std::size_t len) {

                _handleRequest();
            });
        }

        void _handleRequest() {

            std::cout << "_handleRequest(): " << _request.method_string() << " "
                      << _request.target() << std::endl;

            boost::beast::error_code ec;

            if (_server._docRoot) {

                auto filePath = _server._docRoot.value() + std::string{_request.target()};

                if (std::filesystem::exists(filePath)) {

                    beast::http::file_body::value_type body;

                    body.open(filePath.c_str(), beast::file_mode::scan, ec);

                    if (ec) {
                        std::cerr << "failed opening " << filePath << ": " << ec.what() << std::endl;
                        return;
                    }

                    http::response<http::file_body> response{
                            std::piecewise_construct,
                            std::make_tuple(std::move(body)),
                            std::make_tuple(http::status::ok, _request.version())};

                    response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    http::write(_stream, response, ec);

                    if (ec) {
                        std::cerr << ec.what() << std::endl;
                    }

                    return;
                }
            }






//                else {
//                    http::response<http::string_body> response{http::status::not_found, _request.version()};
//                    response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
//                    response.set(http::field::content_type, "text/html");
//                    response.keep_alive(_request.keep_alive());
//                    response.body() = "file not found";
//                    response.prepare_payload();
//                    http::write(_stream, response, ec);
//                }













            // serve a file:





            // serve text:
            /*
            http::response<http::string_body> response{http::status::ok, _request.version()};
            response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            response.set(http::field::content_type, "text/html");
            response.keep_alive(_request.keep_alive());
            response.body() = "hello";
            response.prepare_payload();
            http::write(_stream, response, ec);
            */
        }

        HTTPServer& _server;
        asio::ssl::stream<asio::ip::tcp::socket> _stream;
        beast::flat_buffer _read_buf;
        http::request<http::dynamic_body> _request;
    };

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
};

int main() {

    inja::Environment inja;
    asio::io_service io{};

    auto ssl = initialize_ssl("cert.pem", "key.pem");

    HTTPServer server{io, 8081, ssl};
    server.serveDirectory("doc_root");

    io.run();

    return 0;
}

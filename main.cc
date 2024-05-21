
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

#include "http_server.h"

static std::string read_file(const std::string& file_name) {

    std::ifstream file{file_name};

    if (!file.is_open())
        throw std::runtime_error{"read_file: failed to open " + file_name};

    return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

int main() {

    inja::Environment inja;
    asio::io_service io{};

    asio::ssl::context ssl{asio::ssl::context::tlsv12};

    ssl.set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::single_dh_use);

    boost::system::error_code ec;

    ssl.use_certificate_chain_file("cert.pem", ec);

    if (ec) {
        std::cerr << "use_certificate_chain_file: failed: " << ec.message() << std::endl;
        return -1;
    }

    ssl.use_private_key_file("key.pem", boost::asio::ssl::context::file_format::pem, ec);

    if (ec) {
        std::cerr << "use_private_key_file: failed: " << ec.message() << std::endl;
        return -1;
    }

    HTTPServer server{io, 8080, ssl};

    server.add_route(HTTPServer::Method::GET, "/test.js", [](auto& session) {

        boost::beast::ostream(session.response().body()) << read_file("test.js");
        session.response().content_length(session.response().body().size());
    });

    server.add_route(HTTPServer::Method::GET, "/hello/(\\w+)", [&inja](auto& session) {

        boost::beast::ostream(session.response().body()) << inja.render_file("page.html", {
            { "name", session.url().path_params[0] },
            { "time", std::time(nullptr) }});
        session.response().content_length(session.response().body().size());
    });

    io.run();

    return 0;
}


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

    auto cert = read_file("cert.pem");
    auto key = read_file("key.pem");

    auto ssl = std::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12);

    ssl->set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::single_dh_use);

    ssl->use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));

    ssl->use_private_key(boost::asio::buffer(key.data(), key.size()),
                        boost::asio::ssl::context::file_format::pem);


    HTTPServer server{io, 8080, ssl};

    server.add_route("/test.js", [](auto& session) {

        boost::beast::ostream(session.response().body()) << read_file("test.js");
        session.response().content_length(session.response().body().size());
    });

    server.add_route("/hello/(\\w+)", [&inja](auto& session) {

        boost::beast::ostream(session.response().body()) << inja.render_file("page.html", {
            { "name", session.url().path_params[0] },
            { "time", std::time(nullptr) }});
        session.response().content_length(session.response().body().size());
    });

    io.run();

    return 0;
}

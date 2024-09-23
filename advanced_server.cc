
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

#include "old/http_server.h"

static std::string read_file(const std::string& file_name) {

    std::ifstream file{file_name};

    if (!file.is_open())
        throw std::runtime_error{"read_file: failed to open " + file_name};

    return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

asio::ssl::context initialize_ssl(const std::string& cert_file, const std::string& key_file) {

    asio::ssl::context ssl{asio::ssl::context::tlsv12};

    ssl.set_options(boost::asio::ssl::context::default_workarounds |
                    boost::asio::ssl::context::no_sslv2 |
                    boost::asio::ssl::context::single_dh_use);

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

int main() {

    inja::Environment inja;
    asio::io_service io{};

    auto ssl = initialize_ssl("cert.pem", "key.pem");

    HTTPServer server{io, 8081, ssl};
    server.serve_directory(".");

    /*
    server.add_route(HTTPServer::Method::GET, "/test.js", [](auto& s) {

//        beast::http::file_body::value_type body;
//        std::string doc_root = ".";



        std::string path = doc_root + s.url().path;

        std::cout << path << std::endl;

        beast::error_code ec;
//
        if (ec == beast::errc::no_such_file_or_directory) {
            s.response().result(beast::http::status::not_found);
            return;
        }

        if(ec) {
            s.response().result(beast::http::status::internal_server_error);
            return;
        }
//            return not_found(req.target());

        body.open(path.c_str(), beast::file_mode::scan, ec);

        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, s.request().version())};


//        http::response<http::dynamic_body>& res = s.response();
//        res.body() = "test";
//        res.

//
//        std::cout << s.request().target() << std::endl;

//        s.response() = std::move(res);

//        boost::beast::ostream(s.response().body()) << read_file("test.js");
//        s.response().content_length(s.response().body().size());

    });
    */

    server.add_route(HTTPServer::Method::GET, "/hello/(\\w+)", [&inja](auto& s) {

        std::cout << "hello route" << std::endl;

        http::response<http::string_body> res{http::status::ok, s.request().version()};
        res.body() = "Hello" + s.url().path_params[0];
        res.set(http::field::content_type, "text/html");
        res.prepare_payload();
        s._send_response(std::move(res));

        /*
        boost::beast::ostream(session.response().body()) << inja.render_file("page.html", {
            { "name", session.url().path_params[0] },
            { "time", std::time(nullptr) }});
        session.response().content_length(session.response().body().size());
        */
    });

    io.run();

    return 0;
}

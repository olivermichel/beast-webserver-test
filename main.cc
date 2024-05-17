
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

#include "http_server.h"

int main() {

    inja::Environment inja;
    asio::io_service io{};
    HTTPServer server{io, 8080};

    server.add_route("/test.js", [](auto& session) {

        std::ifstream file("test.js");

        boost::beast::ostream(session.response().body())
            << std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
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

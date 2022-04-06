#include <memory>
#include <cstdlib>
#include <restbed>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include "base64.h"

using namespace std;
using namespace restbed;
using json = nlohmann::json;

struct Auth
{
    string type;
    string username;
    string password;
};

struct RequiredParameter
{
    string upstreamKey;
    string name;
    vector<string> values;
    string defaultVal;
};

Auth GetAuth(const shared_ptr< Session > session)
{
    Auth ret;
    string authorization = session->get_request()->get_header("Authorization");
    size_t breakPos = authorization.find(' ');
    ret.type = authorization.substr(0, breakPos);
    if (ret.type != "Basic")
        return ret;
    string b64 = authorization.substr(++breakPos);
    string b64Decoded;
    macaron::Base64::Decode(b64, b64Decoded);
    breakPos = b64Decoded.find(':');
    ret.username = b64Decoded.substr(0,breakPos);
    ret.password = b64Decoded.substr(++breakPos);
//    fprintf(stdout, "%s (%s:%s => %s)\n", authorization.c_str(), ret.type.c_str(), b64.c_str(), b64Decoded.c_str());
    fprintf(stdout, "Username: \"%s\"\nPassword: \"%s\"\n", ret.username.c_str(), ret.password.c_str());
    return ret;
}

void forward_request(const shared_ptr< Session > session)
{
    const auto request = session->get_request();
    int contentLength = request->get_header("Content-Length", 0);

    session->fetch(contentLength, [ request, contentLength ](const shared_ptr< Session > l_session, const Bytes & l_body)
    {
#if 1
        string parameters;
        for (const auto& param : request->get_query_parameters())
        {
            parameters += "{"+param.first+" : "+param.second+"} ";
        }
        fprintf(stdout, "forwarding package with parameters: {\n    %s\n}\nand l_body (%d): {\n    %.*s\n}\n", parameters.c_str(), contentLength, (int) l_body.size(), l_body.data());
#endif

        // cpr::Response upstreamResponse = cpr::Post(
        //         cpr::Url(Environment::Instance().GetUpstreamURL() + "/wp-json/gf/v2/forms/1/submissions"),
        //         cpr::Body(upstreamRequestBody.dump()),
        //         cpr::Authentication{auth.username, auth.password},
        //         cpr::Header{{"Content-Type", "application/json"}});

        // fprintf(stdout, "Got %ld:\n%s\n", upstreamResponse.status_code, upstreamResponse.text.c_str());
//        l_session->close(upstreamResponse.status_code, upstreamResponse.text, replyHeaders); //<- we don't want to be sending raw responses back to the requester.
        // if (upstreamResponse.status_code == 400 && upstreamResponse.text.find("You do not have access to update this package.") != std::string::npos)
        // {
        //     l_session->close(401, "Unauthorized.");
        //     l_session->erase();
        //     return;
        // }
        // l_session->close(upstreamResponse.status_code, "complete.");
        l_session->close(200, "complete.");
        l_session->erase();
    });
}

int main(const int, const char**)
{
    auto forward = make_shared< Resource >();
    forward->set_path("/forward");
    forward->set_method_handler("POST", forward_request);

    auto settings = make_shared< Settings >();
    settings->set_port(80);
    settings->set_default_header("Connection", "close");

    Service service;
    service.forward(forward);
    service.start(settings);

    return EXIT_SUCCESS;
}
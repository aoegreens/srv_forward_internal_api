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

string GetUrlFromName(string name, string port = "80")
{
    string ret = "http://10.4.";
    string octet_3 = name.substr(1,3);
    octet_3.erase(0, min(octet_3.find_first_not_of('0'), octet_3.size()-1));
    ret += octet_3 + ".";
    string octet_4 = name.substr(1,3);
    octet_4.erase(0, min(octet_4.find_first_not_of('0'), octet_4.size()-1));
    ret += octet_4 + ":" + port;
    return ret;
}

void forward_gpio_get(const shared_ptr< Session > session)
{
    auto request = session->get_request();

#if 1
    string parameters;
    for (const auto& param : request->get_query_parameters())
    {
        parameters += "{"+param.first+" : "+param.second+"} ";
    }
    fprintf(stdout, "Forwarding gpio request for: {\n    %s\n}\n", parameters.c_str());
#endif

    if (!request->has_query_parameter("device"))
    {
        session->close(400, "You must specify a device.");
        session->erase();
        return;
    }

    const vector<RequiredParameter> requiredParameters = {
        {"pin", "pin", {}, ""}, //the gpio to read.
    };

    string upstreamQuery = "?";
    static const string separator = "&";
    for (const auto& req : requiredParameters)
    {
        if (!request->has_query_parameter(req.name))
        {
            if (req.defaultVal.empty())
            {
                session->close(400, "Please specify \"" + req.name + "\"");
                session->erase();
                return;
            }
            if (req.defaultVal != "EMPTY")
            {
                upstreamQuery += req.upstreamKey + "=" + req.defaultVal + separator;
            }
        }
        else
        {
            upstreamQuery += req.upstreamKey + "=" + request->get_query_parameter(req.name) + separator;
        }
    }
    upstreamQuery.substr(0,upstreamQuery.size()-separator.size());

    string upstreamUrl = GetUrlFromName(request->get_query_parameter("device")) + upstreamQuery;

    fprintf(stdout, "Will forward request to %s\n", upstreamUrl.c_str());

    session->close(OK, "ON");
    session->erase();
}

void forward_gpio_post(const shared_ptr< Session > session)
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
        l_session->close(OK, "complete.");
        l_session->erase();
    });
}

int main(const int, const char**)
{
    auto gpio_get = make_shared< Resource >();
    gpio_get->set_path("/gpio");
    gpio_get->set_method_handler("GET", forward_gpio_get);

    auto gpio_post = make_shared< Resource >();
    gpio_post->set_path("/gpio");
    gpio_post->set_method_handler("POST", forward_gpio_post);

    auto settings = make_shared< Settings >();
    settings->set_port(80);
    settings->set_default_header("Connection", "close");

    Service service;
    service.publish(gpio_get);
    service.publish(gpio_post);
    service.start(settings);

    return EXIT_SUCCESS;
}
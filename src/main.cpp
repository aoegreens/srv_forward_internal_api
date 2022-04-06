#include <memory>
#include <cstdlib>
#include <restbed>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include "base64.h"

/**
 * There appears to be a bug in the arm version of the nlohmann json library.
 * We have thus changed all on-device apis to use get methods only.
 */
#define UPSTREAM_USES_JSON 0

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
    string ret = "http://10.1.";
    string octet_3 = name.substr(1,2);
    octet_3.erase(0, min(octet_3.find_first_not_of('0'), octet_3.size()-1));
    ret += octet_3 + ".";
    string octet_4 = name.substr(3,4);
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
        return;
    }

    const vector<RequiredParameter> requiredParameters = {
        {"pin", "pin", {}, ""}, //the gpio to read.
    };

    string upstreamQuery = "/v1/gpio/get?";
    static const string separator = "&";
    for (const auto& req : requiredParameters)
    {
        if (!request->has_query_parameter(req.name))
        {
            if (req.defaultVal.empty())
            {
                session->close(400, "Please specify \"" + req.name + "\"");
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
    upstreamQuery.erase(upstreamQuery.size()-separator.size(), upstreamQuery.size());

    string upstreamUrl = GetUrlFromName(request->get_query_parameter("device")) + upstreamQuery;

    fprintf(stdout, "Will forward request to %s\n", upstreamUrl.c_str());

    cpr::Response upstreamResponse = cpr::Get(cpr::Url{upstreamUrl});

    session->close(upstreamResponse.status_code, upstreamResponse.text);
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

        char* requestJson = new char[l_body.size()];
        sprintf(requestJson, "%.*s", (int) l_body.size(), l_body.data());
        string jStr(requestJson);
        json requestData = json::parse(jStr, nullptr, false);
        if (requestData.is_discarded())
        {
            fprintf(stdout, "Could not parse:\n%.*s\n", (int) l_body.size(), l_body.data());
            l_session->close(400, "Could not parse request. Please submit request as valid json.");
            delete[] requestJson;
            return;
        }
        delete[] requestJson;
        const vector<RequiredParameter> requiredParameters = {
                {"pin", "pin", {}, ""}, //the wiringPi pin to use.
                {"state", "state", {}, ""}, //the state to set; "ON", "OFF". '"TOGGLE'.
        };

        if (!requestData.contains("device"))
        {
            l_session->close(400, "You must specify a device.");
            return;
        }

#if UPSTREAM_USES_JSON
        json upstreamRequestBody;
        for (const auto& req : requiredParameters)
        {
            if (!requestData.contains(req.name))
            {
                if(req.defaultVal.empty())
                {
                    l_session->close(400, "Please specify \"" + req.name + "\"");
                    return;
                }
                if (req.defaultVal == "EMPTY")
                {
                    upstreamRequestBody[req.upstreamKey] = "";
                }
                else
                {
                    upstreamRequestBody[req.upstreamKey] = req.defaultVal;
                }
            }
            else
            {
                upstreamRequestBody[req.upstreamKey] = requestData[req.name];
            }
            fprintf(stdout, "%s (%s): %s\n", req.upstreamKey.c_str(), req.name.c_str(), upstreamRequestBody[req.upstreamKey].get<string>().c_str());
        }
        
        string upstreamUrl = GetUrlFromName(requestData["device"]) + "/v1/gpio/set";

#else
        string upstreamQuery = "/v1/gpio/set?";
        static const string separator = "&";
        for (const auto& req : requiredParameters)
        {
            if (!requestData.contains(req.name))
            {
                if (req.defaultVal.empty())
                {
                    l_session->close(400, "Please specify \"" + req.name + "\"");
                    return;
                }
                if (req.defaultVal != "EMPTY")
                {
                    upstreamQuery += req.upstreamKey + "=" + req.defaultVal + separator;
                }
            }
            else
            {
                string value = requestData[req.name];
                upstreamQuery += req.upstreamKey + "=" + value + separator;
            }
        }
        upstreamQuery.erase(upstreamQuery.size()-separator.size(), upstreamQuery.size());

        string upstreamUrl = GetUrlFromName(requestData["device"]) + upstreamQuery;
#endif  

        fprintf(stdout, "Will forward request to %s\n", upstreamUrl.c_str());

#if UPSTREAM_USES_JSON
        cpr::Response upstreamResponse = cpr::Post(
                cpr::Url(upstreamUrl),
                cpr::Body(upstreamRequestBody.dump()),
                cpr::Header{{"Content-Type", "application/json"}});
#else
        cpr::Response upstreamResponse = cpr::Get(cpr::Url(upstreamUrl));
#endif

        fprintf(stdout, "Got %ld:\n%s\n", upstreamResponse.status_code, upstreamResponse.text.c_str());
        l_session->close(upstreamResponse.status_code, upstreamResponse.text);
        // l_session->close(OK, "complete.");
    });
}

int main(const int, const char**)
{
    auto gpio_get = make_shared< Resource >();
    gpio_get->set_path("/v1/gpio/get");
    gpio_get->set_method_handler("GET", forward_gpio_get);

    auto gpio_post = make_shared< Resource >();
    gpio_post->set_path("/v1/gpio/set");
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
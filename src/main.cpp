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

class Environment
{
public:
    static Environment& Instance()
    {
        static Environment environment;
        return environment;
    }
    string GetUpstreamURL() const {
        return m_upstreamReplacement;
    }
    string MangleURL(string url)
    {
        //assume input is good.
        return GetUpstreamURL() + url.substr(m_upstreamUrl.size());
    }
private:
    Environment() :
        m_upstreamUrl("https://infrastructure.tech"),
        m_upstreamReplacement(getenv("INFRASTRUCTURE_URL"))
    {
    }
    const string m_upstreamUrl;
    const string m_upstreamReplacement;
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

void publish_package(const shared_ptr< Session > session)
{
    const auto request = session->get_request();
    int contentLength = request->get_header("Content-Length", 0);

    session->fetch(contentLength, [ request, contentLength ](const shared_ptr< Session > l_session, const Bytes & l_body)
    {
#if 0
        string parameters;
        for (const auto& param : request->get_query_parameters())
        {
            parameters += "{"+param.first+" : "+param.second+"} ";
        }
        fprintf(stdout, "Publishing package with parameters: {\n    %s\n}\nand l_body (%d): {\n    %.*s\n}\n", parameters.c_str(), contentLength, (int) l_body.size(), l_body.data());
#endif

        if (!request->has_header("Authorization"))
        {
            l_session->close(401, "Publishing requires http basic auth with username and password.");
            l_session->erase();
            return;
        }
        Auth auth = GetAuth(l_session);
        if (auth.username.empty() || auth.password.empty())
        {
            l_session->close(401, "Publishing requires http basic auth with username and password.");
            l_session->erase();
            return;
        }
        //TODO: why is this requestJson string nonsense required in order to trim invalid bytes from the end of the 2nd and onward calls of json::parse()???
        char* requestJson = new char[l_body.size()];
        sprintf(requestJson, "%.*s", (int) l_body.size(), l_body.data());
        string jStr(requestJson); //= R"({"package_name": "bin_cpp_api_test", "version": "v0.0.3", "visibility": "private", "package": ""})"; // <-  static string works 100%.
//        json requestData = json::parse(l_body.data(), nullptr, false); //parsing bytes only works the 1st time; even though the prints are valid.
        json requestData = json::parse(jStr, nullptr, false);
        if (requestData.is_discarded())
        {
            fprintf(stdout, "Could not parse:\n%.*s\n", (int) l_body.size(), l_body.data());
            l_session->close(400, "Could not parse request. Please submit request as valid json.");
            l_session->erase();
            return;
        }
        delete[] requestJson;
        //{(ignore this bit), "parameter name", {possible choices}, "default value"}
        const vector<RequiredParameter> requiredParameters = {
                {"input_1", "package_name", {}, ""}, //the name of the package
                {"input_2", "version", {}, ""}, //the version of the package
                {"input_3", "package", {}, ""}, //the package data, base64 encoded.
                {"input_4", "visibility", {"public", "private"}, "private"}, //"private" if the package requires authentication to view; "public" if it does not.
                {"input_5", "package_type", {}, "EMPTY"}, //metadata. use as thou wilt.
                {"input_6", "description", {}, "EMPTY"} //docs maybe?
        };

        json upstreamRequestBody;
        for (const auto& req : requiredParameters)
        {
            if (!requestData.contains(req.name))
            {
                if(req.defaultVal.empty())
                {
                    l_session->close(400, "Please specify \"" + req.name + "\"");
                    l_session->erase();
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
        if (upstreamRequestBody["input_4"] == "public")
            upstreamRequestBody["input_4"] = "publish"; //what upstream expects.

//        fprintf(stdout, "Request l_body:\n%s\n", upstreamRequestBody.dump().c_str());

        cpr::Response upstreamResponse = cpr::Post(
                cpr::Url(Environment::Instance().GetUpstreamURL() + "/wp-json/gf/v2/forms/1/submissions"),
                cpr::Body(upstreamRequestBody.dump()),
                cpr::Authentication{auth.username, auth.password},
                cpr::Header{{"Content-Type", "application/json"}});

//        multimap< string, string > replyHeaders;
//        for (const auto& head : upstreamResponse.header)
//        {
//            replyHeaders.insert(pair<string,string>(head.first, head.second));
//            fprintf(stdout, "Response header: {%s : %s}\n", head.first.c_str(), head.second.c_str());
//        }

//        const multimap< string, string > replyHeaders
//        {
//            { "Content-Type", "application/json" }
//        };

        fprintf(stdout, "Got %ld:\n%s\n", upstreamResponse.status_code, upstreamResponse.text.c_str());
//        l_session->close(upstreamResponse.status_code, upstreamResponse.text, replyHeaders); //<- we don't want to be sending raw responses back to the requester.
        if (upstreamResponse.status_code == 400 && upstreamResponse.text.find("You do not have access to update this package.") != std::string::npos)
        {
            l_session->close(401, "Unauthorized.");
            l_session->erase();
            return;
        }
        l_session->close(upstreamResponse.status_code, "complete.");
        l_session->erase();
    });
}

void download_package(const shared_ptr< Session > session)
{
    auto request = session->get_request();

#if 1
    string parameters;
    for (const auto& param : request->get_query_parameters())
    {
        parameters += "{"+param.first+" : "+param.second+"} ";
    }
    fprintf(stdout, "Getting package with parameters: {\n    %s\n}\n", parameters.c_str());
#endif

    Auth auth = GetAuth(session);
    if (!request->has_query_parameter("package_name"))
    {
        session->close(400, "You must specify the \"package_name\" that you would like to download.");
    }

    string packageName = request->get_query_parameter("package_name");
    string public_url = Environment::Instance().GetUpstreamURL() + "/wp-json/wp/v2/package?slug=" + packageName;
    string private_url = public_url + "&status=private";
    cpr::Response upstreamResponse;
    if (request->has_header("Authorization"))
    {
        upstreamResponse = cpr::Get(cpr::Url{private_url}, cpr::Authentication{auth.username, auth.password});
        if (upstreamResponse.text == "[]")
        {
            upstreamResponse = cpr::Get(cpr::Url{public_url});
        }
    }
    else
    {
        upstreamResponse = cpr::Get(cpr::Url{public_url});
    }

    fprintf(stdout, "got %ld: %s\n", upstreamResponse.status_code, upstreamResponse.text.c_str());
    if (upstreamResponse.status_code == 401)
    {
        session->close(401, "Unauthorized. Wrong password?");
        session->erase();
        return;
    }

    json responseData = json::parse(upstreamResponse.text);
    if (responseData.size() != 1 || !responseData[0].contains("file"))
    {
        session->close(404, "Package " + packageName + " could not be found.");
        session->erase();
        return;
    }
    string fileUrl = responseData[0]["file"].get<string>();
//    fprintf(stdout, "json: %s\n", responseData.dump().c_str());
//    fprintf(stdout, "file: %s\n", fileUrl.c_str());
    if (fileUrl.empty())
    {
        session->close(404, "Package " + packageName + " could not be found.");
        session->erase();
        return;
    }
    string mangledFileUrl = Environment::Instance().MangleURL(fileUrl);
    fprintf(stdout, "returning: %s\n", mangledFileUrl.c_str());
    cpr::Response fileData = cpr::Get(cpr::Url{mangledFileUrl});

    const multimap< string, string > replyHeaders
    {
        { "Content-Disposition", "attachment; filename=\""+packageName+".zip\"" },
        { "Content-Type", "application/force-download" }
    };

    session->close(OK, fileData.text, replyHeaders);
    session->erase();
}

int main(const int, const char**)
{
    auto publish = make_shared< Resource >();
    publish->set_path("v1/package/publish");
    publish->set_method_handler("POST", publish_package);

    auto download = make_shared< Resource >();
    download->set_path("v1/package/download");
    download->set_method_handler("GET", download_package);

    auto settings = make_shared< Settings >();
    settings->set_port(80);
    settings->set_default_header("Connection", "close");

    Service service;
    service.publish(publish);
    service.publish(download);
    service.start(settings);

    return EXIT_SUCCESS;
}
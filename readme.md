# Web API For Infrastructure.Tech

![build](https://github.com/infrastructure-tech/srv_infrastructure/actions/workflows/ebbs-build.yml/badge.svg)

## Usage

This api is running on https://api.infrastructure.tech.  
It has been made public for transparency and community feedback. If you find it useful for your own projects, fork or just copy it!  
Be aware that a clone of this api server should not function outside the [Web Infrastructure](https://web.infrastructure.tech) hosting platform as the internal requests bypass the public-facing firewalls.

This API will grow as more is added to [Infrastructure Tech](https://infrastructure.tech).  
More documentation will be coming later.

### User Account
You must have an infrastructure.tech account in order to publish to this repository. At this time, there is no sign-up page. If you would like an account, please email support@infrastructure.tech and we'll be happy to work with you to make sure our software meets your needs :)

### Package Repository

You can publish packages (zip files) to, and download them from, the infrastructure.tech repository using this api.  
This is especially useful for other [eons](https://eons.dev) and Web Infrastructure projects. For an example of how you might use this functionality, see how it is implemented in [the eons basic build system](https://github.com/eons-dev/ebbs).

There are 2 main methods for handling packages: publish (POST) and download (GET).
Both queries take HTTP Basic Auth headers (i.e. username and password). While authentication is necessary for publishing, it is not required for downloading. However, if you wish to download private packages, you must authenticate. Additionally, public packages will be searched if no private package is found. So, you can supply your username and password with every request.
Here's the methods:
```c++
void publish_package(const shared_ptr< Session > session )
{
    ...
    //{(ignore this bit), "parameter name", {possible choices}, "default value"}
    const vector<RequiredParameter> requiredParameters = {
        {, "package_name", {}, ""}, //the name of the package
        {, "version", {}, ""}, //the version of the package
        {, "package", {}, ""}, //the package data, base64 encoded.
        {, "visibility", {"public", "private"}, "private"}, //"private" if the package requires authentication to view; "public" if it does not.
        {, "package_type", {}, "EMPTY"}, //metadata. use as thou wilt.
        {, "description", {}, "EMPTY"} //docs maybe?
    };
    ...
}
void download_package(const shared_ptr< Session > session )
{
    ...
}
```
The associated URLs are:
```c++
    auto publish = make_shared<Resource>();
    publish->set_path("v1/package/publish" );
    publish->set_method_handler("POST", publish_package );

    auto download = make_shared<Resource>();
    download->set_path("v1/package/download" );
    download->set_method_handler("GET", download_package );
```


When using `download_package`, you may specify `username` and `password` as you would for `publish_package`. Doing so will cause the api to search private packages before public ones. If no private nor public package is fround a 404 will be returned.

## Design

Originally, this API was written in python using Django. However, because eons LLC (the parent organization of infrastructure.tech) shifted to a c++ focused toolstack with its release of [Biology](https://develop.bio), it made more sense to migrate the APIs used by Infrastructure to a c++ web framework for easy maintenance and consistent design.

For this project, we currently use:
 * [Restbed](https://github.com/Corvusoft/restbed)
 * [nlohmann/json](https://github.com/nlohmann/json)
 * [CPR](https://github.com/libcpr/cpr)
 * [tomykaira/Base64.h](https://gist.github.com/tomykaira/f0fd86b6c73063283afe550bc5d77594)

All compiled dependencies are rolled into the [eons webserver docker image](https://hub.docker.com/repository/docker/eons/webserver), which then runs this app.

## Setup

If you would like to copy this design and run your own c++ webservers, it's really quite easy.
If you don't have Restbed, json, and CPR installed, go build them from the above links (you should be able to just `git clone...; mkdir build; cd build; cmake ..; make; make install`).

### With EBBS

The easiest way to build any eons project is with [ebbs](https://github.com/eons-dev/ebbs). To do so, run:
```shell
cd /path/to/code/folder
pip install ebbs
ebbs -l cpp ./build
```
To run,
```shell
./build/out/entrypoint
```
NOTE: "entrypoint" is specified in the config.json.

### Without EBBS

All ebbs really does is dynamically generate a cmake file for us. So, if you'd like to build this project without it, you'll just want to create a cmake file (or use whatever other build system you'd like).

A cmake produced by ebbs might look like:
```cmake

cmake_minimum_required (VERSION 3.1.1)
set (CMAKE_CXX_STANDARD 17)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY /home/eons/git/bin_cpp_api_test/./build/entrypoint)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY /home/eons/git/bin_cpp_api_test/./build/entrypoint)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY /home/eons/git/bin_cpp_api_test/./build/entrypoint)
project (entrypoint)
include_directories(/home/eons/git/bin_cpp_api_test/inc)
add_executable (entrypoint /home/eons/git/bin_cpp_api_test/src/main.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(Threads::Threads)
target_link_libraries(entrypoint restbed cpr)

```
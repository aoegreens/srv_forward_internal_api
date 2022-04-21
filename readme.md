# Internal API Request Forwarder

This plugin is part of a pipeline used to control AOE Greens grow racks through WordPress.

We have provided this code to you in the hopes of making sustainable food production a world-wide reality. For more information on our open source software and how to reach us, please visit [https://aoegreens.com/about/](https://aoegreens.com/about/).

## Usage

This forwarding server is intended to take hardware requests (e.g. read or write a GPIO) and forward them to the correct device.

To see how this server can be used, please see [the associated WordPress plugin](https://github.com/aoegreens/plugin_rack-panel).

Here are some useful snippets from that code:

Reading a GPIO:
```php
$url = "http://forwarder-1-svc:80/v1/gpio/get?device=".$args["device"]."&pin=".$args["gpio"];
$curl = curl_init($url);
curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
$result = curl_exec($curl);
curl_close($curl);
```

Write a GPIO (Toggle it):
```php
$url = "http://forwarder-1-svc:80/v1/gpio/set";
$payload = json_encode(array(
    "device" => $args["device"],
    "pin" => $args["gpio"],
    "state" => "TOGGLE"
));
$curl = curl_init($url);
curl_setopt($curl, CURLOPT_POST, true);
curl_setopt($curl, CURLOPT_POSTFIELDS, $payload);
curl_setopt($curl, CURLOPT_HTTPHEADER, array('Content-Type:application/json'));
curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
$result = curl_exec($curl);
curl_close($curl);
```

This forwarder is intended to interface with RESTful API servers running on micro controllers and other devices. For more information and an example, please see [the Raspberry Pi control server](https://github.com/aoegreens/srv_pi_controller).

## Building

This code is intended to be built with [EBBS](https://github.com/eons-dev/bin_ebbs).

### Dependencies

Before building this, make sure you have built or installed the following dependencies

For this project, we currently use:
 * [Restbed](https://github.com/Corvusoft/restbed)
 * [CPR](https://github.com/libcpr/cpr)
 * [nlohmann/json](https://github.com/nlohmann/json)
 * [tomykaira/Base64.h](https://gist.github.com/tomykaira/f0fd86b6c73063283afe550bc5d77594) (Included in project)

For our use, we install this app alongside a vpn within a virtual machine, which allows for more secure access to our hardware.

### With EBBS

To build this code locally, you can run:
```shell
pip install ebbs
ebbs
```
Make sure that you are in the root of this repository, where the `build.json` file lives.

This code is intended to be built with [EBBS](https://github.com/eons-dev/bin_ebbs).

### Without EBBS

All ebbs really does is dynamically generate a cmake file for us. So, if you'd like to build this project without it, you'll just want to create a cmake file (or use whatever other build system you'd like).

A cmake produced by ebbs might look like:
```cmake

cmake_minimum_required (VERSION 3.1.1)
set (CMAKE_CXX_STANDARD 17)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY /opt/git/srv_forward_internal_api/./build/entrypoint)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY /opt/git/srv_forward_internal_api/./build/entrypoint)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY /opt/git/srv_forward_internal_api/./build/entrypoint)
project (entrypoint)
include_directories(/opt/git/srv_forward_internal_api/inc)
add_executable (entrypoint /opt/git/srv_forward_internal_api/src/main.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(Threads::Threads)
target_link_libraries(entrypoint restbed cpr)

```
#include <libwebsockets.h>
#include <iostream>

int main() {
    std::cout << "libwebsockets version: " << lws_get_library_version() << std::endl;
    return 0;
}

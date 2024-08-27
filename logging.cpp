#include <iostream>
#include <string>

void print_debug(std::string str) {
    print_debug(str.c_str());
}

void print_debug(const char *str) {
#ifdef DEBUG_CLUSTER
    std::cout << str;
#endif
}

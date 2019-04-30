#include "longopt.h"

#include <iostream>

static const option_t kOptions[] = {
    { '-', NULL, 0, NULL, "arguments:" },
    { 'l', "listen-udp", LONGOPT_REQUIRE, NULL, "" },
    { 'L', "listen-tcp", LONGOPT_REQUIRE, NULL, "" },
    { 0, NULL, 0, NULL, NULL }
};

int main(int argc, char* argv[]) {
    print_opt(kOptions);
    return 0;
}
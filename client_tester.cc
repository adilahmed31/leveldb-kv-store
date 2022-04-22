#include <grpcpp/grpcpp.h>
#include <sys/stat.h>
#include <time.h>

#include <iostream>

#include "client.cc"
#include "wifs.grpc.pb.h"

void tester() {
    char buf[BLOCK_SIZE + 1];
    for (int i = 0; i < BLOCK_SIZE; i++) buf[i] = 'X';
    int rc = do_put(0, buf);
    if (rc == -1) std::cout << "PUT FAIL\n";

    buf[0] = '\0';
    rc = do_get(0, buf);
    if (rc == -1) std::cout << "GET FAIL\n";

    buf[BLOCK_SIZE] = '\0';
    printf("get first char - %c\n", buf[0]);

}

int main(int argc, char* argv[]) {
    init();
    tester();
    return 0;
}

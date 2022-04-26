#include <grpcpp/grpcpp.h>
#include <sys/stat.h>
#include <time.h>

#include <iostream>

#include "client.cc"
#include "wifs.grpc.pb.h"

void tester(char* key) {
    char buf[BLOCK_SIZE + 1];
    // for (int i = 0; i < BLOCK_SIZE; i++) buf[i] = 'Z';
    // int rc = do_put(key, buf);
    // if (rc == -1) std::cout << "PUT FAIL\n";

    buf[0] = '\0';
    int rc = do_get(key, buf);
    if (rc == -1) std::cout << "GET FAIL\n";

    buf[BLOCK_SIZE] = '\0';
    printf("get first char - %c\n", buf[0]);

}

int main(int argc, char* argv[]) {
    char* key = (char*)"4";
    tester(key);
    key = (char*)"400";
    tester(key);
    key = (char*)"4000";
    tester(key);
    key = (char*)"40000";
    tester(key);
    key = (char*)"400000";
    tester(key);
    return 0;
}

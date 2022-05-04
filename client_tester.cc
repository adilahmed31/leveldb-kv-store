#include <grpcpp/grpcpp.h>
#include <sys/stat.h>
#include <time.h>

#include <iostream>

#include "client.cc"
#include "wifs.grpc.pb.h"

void tester(char* key) {
    char* buf = (char*)malloc(INT_MAX);
    for (int i = 0; i < BLOCK_SIZE; i++) buf[i] = 'Z';
    int rc;
    rc = do_put(key, buf);
    if (rc == -1) std::cout << "PUT FAIL\n";

    buf[0] = '\0';
    rc = do_get(key, buf);
    if (rc == -1) std::cout << "GET FAIL\n";
    
    // buf[BLOCK_SIZE] = '\0';
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
    key = (char*)"l1fsdf";
    tester(key);
    return 0;
}

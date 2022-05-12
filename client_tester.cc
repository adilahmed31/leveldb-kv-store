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
    // rc = do_get(key, buf);
    rc = do_get(key, buf);

    if (rc == -1) std::cout << "GET FAIL\n";
    
    buf[BLOCK_SIZE] = '\0';
    printf("get first char - %c\n", buf[0]);
}

void group_tester() {
    char* buf = (char*)malloc(INT_MAX);
    for (int i = 0; i < BLOCK_SIZE; i++) buf[i] = 'Z';
    int rc;
    char* key = (char*)"kalyani4";
    rc = do_put(key, buf);
    key = (char*)"kalyani4400";
    rc = do_put(key, buf);
    key = (char*)"kalyani44000";
    rc = do_put(key, buf);
    key = (char*)"kalyani440000";
    rc = do_put(key, buf);
    key = (char*)"kalyani4400000";
    rc = do_put(key, buf);

    if (rc == -1) std::cout << "PUT FAIL\n";

    buf[0] = '\0';
    // rc = do_get(key, buf);
    std::vector<wifs::KVPair> batch_read;
    rc = do_getRange("kalyani*", &batch_read);
    std::cout << "size(batch_read) "  << batch_read.size() << std::endl;

    for (int i=0; i<  batch_read.size(); i++ ){
        std::cout << "print" << std::endl;
        std::cout << batch_read[i].key()  << ' '  << std::endl;
    }
    if (rc == -1) std::cout << "GET FAIL\n";
    
    // buf[BLOCK_SIZE] = '\0';
    // printf("get first char - %c\n", buf[0]);

}

int main(int argc, char* argv[]) {

    char* key = (char*)"k12";
    tester(key);
    key = (char*)"e34";
    tester(key);
    key = (char*)"h76";
    tester(key);
    key = (char*)"l89";
    tester(key);
    key = (char*)"dshb";
    tester(key);
    key = (char*)"otg";
    tester(key);

    // group_tester();
    return 0;
}

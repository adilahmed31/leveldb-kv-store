#include <grpcpp/grpcpp.h>
#include <sys/stat.h>
#include <time.h>

#include <iostream>

#include "client.cc"
#include "wifs.grpc.pb.h"

void tester(char* key) {
    char* buf = (char*)malloc(BLOCK_SIZE);
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
    char* buf = (char*)malloc(BLOCK_SIZE);
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
    if(argc > 1) {
        zk_server_ip = (char*)(argv[1]);
    }
    char* key = (char*)"kalyani4";
    tester(key);
    key = (char*)"kalyani4400";
    tester(key);
    key = (char*)"kalyani44000";
    tester(key);
    key = (char*)"kalyani440000";
    tester(key);
    key = (char*)"kalyani4400000";
    tester(key);
    key = (char*)"l1fsdf";
    tester(key);

    // group_tester();
    return 0;
}
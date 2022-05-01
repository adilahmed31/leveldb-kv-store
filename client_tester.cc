#include <grpcpp/grpcpp.h>
#include <sys/stat.h>
#include <time.h>

#include <iostream>

#include "client.cc"
#include "wifs.grpc.pb.h"

void tester(char* key) {
    char* buf = (char*)malloc(INT_MAX);
    // for (int i = 0; i < BLOCK_SIZE; i++) buf[i] = 'Z';
    // int rc;
    // rc = do_put(key, buf);
    // if (rc == -1) std::cout << "PUT FAIL\n";

    // buf[0] = '\0';
    int rc = do_get(key, buf);
    if (rc == -1) std::cout << "GET FAIL\n";
    
    // buf[BLOCK_SIZE] = '\0';
    printf("get all chars - %s\n", buf);

}

int main(int argc, char* argv[]) {

    // needs to know master 
    std::string tmp_master_ip = "localhost";
    wifs::ServerDetails master_details;
    master_details.set_serverid(0);
    master_details.set_ipaddr(tmp_master_ip);
    server_map[somehashfunction(tmp_master_ip)] = master_details;

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

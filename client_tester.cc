#include <grpcpp/grpcpp.h>
#include <sys/stat.h>
#include <time.h>

#include <iostream>

#include "client.cc"
#include "wifs.grpc.pb.h"


#include <ctime>
#include <unistd.h>

std::string gen_random(const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    
    return tmp_s;
}

void do_put_for_key(char* key) {
    std::string val = gen_random(5);
    do_put(key, (const char *) val.c_str());
    std::cout<<"PUT k="<<key<<" val="<<val<<"\n";
}

void do_get_for_key(char* key) {
    char buf[20];
    int rc = do_get(key, buf);
    printf("GOT k=%s val=%s\n", key, buf);
}

int main(int argc, char* argv[]) {
    srand((unsigned)time(NULL) * getpid());  
    if(argc < 2) {
        printf("run with mode. 1 for put and 2 for get\n");
        exit(0);
    }

    int mode = atoi(argv[1]);
    if(mode == 1) {
        char* key = (char*)"k12";
        do_put_for_key(key);
        key = (char*)"e34";
        do_put_for_key(key);
        key = (char*)"h76";
        do_put_for_key(key);
        key = (char*)"l89";
        do_put_for_key(key);
        key = (char*)"dshb";
        do_put_for_key(key);
        key = (char*)"otg";
        do_put_for_key(key);
        return 0;
    }

    char* key = (char*)"k12";
    do_get_for_key(key);
    key = (char*)"e34";
    do_get_for_key(key);
    key = (char*)"h76";
    do_get_for_key(key);
    key = (char*)"l89";
    do_get_for_key(key);
    key = (char*)"dshb";
    do_get_for_key(key);
    key = (char*)"otg";
    do_get_for_key(key);
    return 0;
}

#define MAX_PATH_LENGTH 1000
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#define HEARTBEAT_TIMER 1000
#define RINGLENGTH 4
#define MAX_NUM_SERVERS 100 //limiting max servers to use static array for client_stub - can be depreciated if client_stub need not be reused

//Consistent hashing - modify/replace hash function if required
unsigned int somehashfunction(unsigned int x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    //printf("%d: x before\n", x);
    x = x % RINGLENGTH;
    //printf("%d: x after\n", x);
    return x ;
}

// fix this
std::string ip_servers_p2p[4] = {"localhost:50060","localhost:50061","localhost:50062","localhost:50063"};
std::string ip_server_wifs[4] = {"localhost:50070","localhost:50071","localhost:50072","localhost:50073"};

int read_index = 0;
int primary_index = 0;
int single_server = 0;

std::string getServerDir(int machine_id){
        return "/users/oahmed4/.server" +  std::to_string(machine_id);
}

std::string getServerPath(int machine_id) {
    return getServerDir(machine_id) + "/kv" ;//"/file_" + address;
}

std::string getLastAddressPath(int machine_id) {
    return getServerDir(machine_id) + "/lastaddr" ;//"/file_" + address;
}

std::string getP2PServerPort(int machine_id){
        return "localhost:" +  std::to_string(50060+machine_id);
}

std::string getWifsServerPort(int machine_id){
        return "localhost:" +  std::to_string(50070+machine_id);
}
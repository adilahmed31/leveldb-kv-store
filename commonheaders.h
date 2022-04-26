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
#include <sys/types.h>
#include <pwd.h>
#include <iostream>

#define MAX_PATH_LENGTH 1000
#define HEARTBEAT_TIMER 1000
#define RINGLENGTH 255

//limiting max servers to use static array for client_stub - can be depreciated if client_stub need not be reused
#define MAX_NUM_SERVERS 100 

int read_index = 0;
int primary_index = 0;
int single_server = 0;

std::string getServerDir(int machine_id){
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
        return std::string(homedir) + "/.server" + std::to_string(machine_id);
}

std::string getP2PServerAddr(int machine_id){
        return "localhost:" +  std::to_string(50060+machine_id);
}

std::string getWifsServerAddr(int machine_id){
        return "localhost:" +  std::to_string(50070+machine_id);
}

//Consistent hashing - modify/replace hash function if required
unsigned int somehashfunction(std::string s) {
    std::size_t x = std::hash<std::string>{}(s);
    x = x+8349;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    // printf("%d: x before\n", x);
    x = x % RINGLENGTH;
    // printf("%d: x after\n", x);
    return x ;
}

//Save list of servers in ascending order of ranges of keys handled
std::map<long,int> server_map;

void insert_server_entry(int server_id){
  server_map[somehashfunction(std::to_string(server_id))] = server_id;
}

void remove_server_entry(int server_id){
    server_map.erase(somehashfunction(std::to_string(server_id)));
}

void print_ring(){
    for(auto it = server_map.begin() ; it != server_map.end() ; it++) {
        std::cout<<it->first<<" - "<<it->second<<std::endl;
    }
}


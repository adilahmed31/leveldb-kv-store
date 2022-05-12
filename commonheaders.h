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
#include <fstream>
#include <experimental/filesystem>
#include "wifs.grpc.pb.h"
using wifs::ServerDetails;

#define MAX_PATH_LENGTH 1000
#define HEARTBEAT_TIMER 1000
#define RINGLENGTH 255

std::string MASTER_IP = "";

//limiting max servers to use static array for client_stub - can be depreciated if client_stub need not be reused
#define MAX_NUM_SERVERS 100 

int read_index = 0;
int primary_index = 0;
int single_server = 0;
std::string master_file;
int MASTER_ID = 0;

std::string efs_mount_path = "";

std::string getCacheDir(int machine_id){
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    return std::string(homedir) + "/.cache" + std::to_string(machine_id);
}

std::string getHomeDir() {
    // if a mount path is specified, just use that
    if(efs_mount_path != "") return efs_mount_path;
    else{
        const char *homedir;
        if ((homedir = getenv("HOME")) == NULL) {
            homedir = getpwuid(getuid())->pw_dir;
        }
        return std::string(homedir);
    }
}

std::string getServerDir(int machine_id){
    return getHomeDir() + "/.server" + std::to_string(machine_id);
}

std::string getP2PServerAddr(wifs::ServerDetails sd){
        return sd.ipaddr() + ":"+ std::to_string(50060 + sd.serverid());
}

std::string getWifsServerAddr(wifs::ServerDetails sd){
        return sd.ipaddr() + ":"+ std::to_string(50170 + sd.serverid());
}

//Consistent hashing - modify/replace hash function if required
unsigned int somehashfunction(std::string s) {
    std::size_t x = std::hash<std::string>{}(s);
    x = x % RINGLENGTH;
    return x ;
}

//Save list of servers in ascending order of ranges of keys handled
std::map<long,wifs::ServerDetails> server_map;

void insert_server_entry(wifs::ServerDetails sd){
    server_map[somehashfunction(getP2PServerAddr(sd))] = sd;
}

void remove_server_entry(wifs::ServerDetails sd){
    server_map.erase(somehashfunction(getP2PServerAddr(sd)));
}

void print_ring(){
    for(auto it = server_map.begin() ; it != server_map.end() ; it++) {
        std::cout<<"h:"<<it->first<<"-id:"<<it->second.serverid()<<" ";
    }
    std::cout<<"\n";
}


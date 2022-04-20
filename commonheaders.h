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

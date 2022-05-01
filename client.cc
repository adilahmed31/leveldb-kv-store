#include <grpcpp/grpcpp.h>
#include <time.h>

#include "WifsClient.h"
#include "wifs.grpc.pb.h"

static struct options {
    WifsClient* wifsclient[MAX_NUM_SERVERS];
    int show_help;
} options;

void init_tmp_master(){
    // needs to know master 
    std::string tmp_master_ip = "localhost";
    wifs::ServerDetails master_details;
    master_details.set_serverid(0);
    master_details.set_ipaddr(tmp_master_ip);
    server_map[somehashfunction(tmp_master_ip)] = master_details;
}

extern "C" {
    int init(wifs::ServerDetails details) {
        options.wifsclient[details.serverid()] = new WifsClient(grpc::CreateChannel(getWifsServerAddr(details), grpc::InsecureChannelCredentials()));
    }

    int do_get(char* key, char* val) {
        if (server_map.empty()){
            init_tmp_master();
        }
        auto it = server_map.lower_bound(somehashfunction(std::string(key)));
        if(it == server_map.end()) it = server_map.begin();
        if(options.wifsclient[it->second.serverid()] == NULL) init(it->second);
        int rc = options.wifsclient[it->second.serverid()]->wifs_GET(key, val);
        return rc;
    }

    int do_put(char* key, char* val) {
        if (server_map.empty()){
            init_tmp_master();
        }
        auto it = server_map.lower_bound(somehashfunction(std::string(key)));
        if(it == server_map.end()) it = server_map.begin();
        if(options.wifsclient[it->second.serverid()] == NULL) init(it->second);
        int rc = options.wifsclient[it->second.serverid()]->wifs_PUT(key, val);
        return rc;
    }
}

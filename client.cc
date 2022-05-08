#include <grpcpp/grpcpp.h>
#include <time.h>

#include "WifsClient.h"
#include "wifs.grpc.pb.h"

static struct options {
    WifsClient* wifsclient[MAX_NUM_SERVERS];
    int show_help;
} options;

void populate_tmp_master_server_details(wifs::ServerDetails &master_details) {
    master_details.set_serverid(0);
    char arr[500];
    gethostname(arr, 500);
    master_details.set_ipaddr(arr);
}

void init_tmp_master(){
    // needs to know master 
    wifs::ServerDetails master_details;
    populate_tmp_master_server_details(master_details);
    server_map[somehashfunction(getP2PServerAddr(master_details))] = master_details;
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

    int do_delete(char* key) {
        if (server_map.empty()){
            init_tmp_master();
        }
        auto it = server_map.lower_bound(somehashfunction(std::string(key)));
        if (it == server_map.end()) it = server_map.begin();
        if (options.wifsclient[it->second.serverid()] == NULL) init(it->second);
        int rc = options.wifsclient[it->second.serverid()]->wifs_DELETE(key);
        return rc;
    }
}

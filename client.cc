#include <grpcpp/grpcpp.h>
#include <time.h>

#include "WifsClient.h"
#include "wifs.grpc.pb.h"

static struct options {
    WifsClient* wifsclient[MAX_NUM_SERVERS];
    int show_help;
} options;


int init(int id) {
    options.wifsclient[id] = new WifsClient(grpc::CreateChannel(getWifsServerAddr(id), grpc::InsecureChannelCredentials()));
}

int do_get(char* key, char* val) {
    auto it = server_map.lower_bound(somehashfunction(std::string(key)));
    if(options.wifsclient[it->second] == NULL) init(it->second);
    int rc = options.wifsclient[it->second]->wifs_GET(key, val);
    return rc;
}

int do_put(int key, char* val) {
    auto it = server_map.lower_bound(somehashfunction(std::string(key)));
    if(options.wifsclient[it->second] == NULL) init(it->second);
    int rc = options.wifsclient[it->second]->wifs_PUT(key, val);
    return rc;
}

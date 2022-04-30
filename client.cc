#include <grpcpp/grpcpp.h>
#include <time.h>

#include "WifsClient.h"
#include "wifs.grpc.pb.h"

static struct options {
    WifsClient* wifsclient[MAX_NUM_SERVERS];
    int show_help;
} options;

extern "C" {
    int init(wifs::ServerDetails details) {
        options.wifsclient[details.serverid()] = new WifsClient(grpc::CreateChannel(getWifsServerAddr(details), grpc::InsecureChannelCredentials()));
    }

    int do_get(char* key, char* val) {
        auto it = server_map.lower_bound(somehashfunction(std::string(key)));
        if(options.wifsclient[it->second.serverid()] == NULL) init(it->second);
        int rc = options.wifsclient[it->second.serverid()]->wifs_GET(key, val);
        return rc;
    }

    int do_put(char* key, char* val) {
        auto it = server_map.lower_bound(somehashfunction(std::string(key)));
        if(options.wifsclient[it->second.serverid()] == NULL) init(it->second);
        int rc = options.wifsclient[it->second.serverid()]->wifs_PUT(key, val);
        return rc;
    }
}

#include <grpcpp/grpcpp.h>
#include <time.h>
#include <unistd.h>

#include "WifsClient.h"
#include "wifs.grpc.pb.h"

#include <conservator/ConservatorFrameworkFactory.h>
#include <conservator/ConservatorFramework.h>
#include <conservator/ExistsBuilder.h>

unique_ptr<ConservatorFramework> framework;
char* zk_server_ip = "127.0.0.1:2181";

static struct options {
    WifsClient* wifsclient[MAX_NUM_SERVERS];
    int show_help;
} options;

void init_zk_connection(char* zk_server_ip) {
    ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
    framework = factory.newClient(zk_server_ip,1000);
    framework->start();
}

void populate_tmp_master_server_details(wifs::ServerDetails &master_details) {
    if(framework->checkExists()->forPath("/master") == ZOK) {
        std::cout<<"ZOK\n";
        std::string master_server_details_str = framework->getData()->forPath("/master");
        std::cout<<master_server_details_str<<"\n";
        int delim_pos = (int) master_server_details_str.find(":");
        master_details.set_ipaddr(master_server_details_str.substr(0, delim_pos));
        std::cout<<master_details.ipaddr()<<"\n";
        master_details.set_serverid(atoi(master_server_details_str.substr(delim_pos+1, 5).c_str()) - 50060);
        std::cout<<master_details.serverid()<<"\n";
        return;
    } 
    std::cout<<"zk doesn't have master ip, something is wrong. exit\n";
    std::exit(0);
}

void init_tmp_master(){
    // needs to know master 
    wifs::ServerDetails master_details;
    populate_tmp_master_server_details(master_details);
    std::cout<<getP2PServerAddr(master_details)<<"\n";
    server_map[somehashfunction(getP2PServerAddr(master_details))] = master_details;
}

void reset_client_cache() {
    server_map.clear();
    framework->close();
}

extern "C" {;
    int set_zk_ip(char* zk_ip){
        zk_server_ip = zk_ip;
    };

    int init(wifs::ServerDetails details) {
        options.wifsclient[details.serverid()] = new WifsClient(grpc::CreateChannel(getWifsServerAddr(details), grpc::InsecureChannelCredentials()));
    }

    int do_get(char* key, char* val) { //mode 0 for default, 1 for batch reads
        if (server_map.empty()){
            init_zk_connection(zk_server_ip);
            init_tmp_master();
        }
        auto it = server_map.lower_bound(somehashfunction(std::string(key)));
        if(it == server_map.end()) it = server_map.begin();
        if(options.wifsclient[it->second.serverid()] == NULL) init(it->second);
        int rc = options.wifsclient[it->second.serverid()]->wifs_GET(key, val);
        if(rc < 0) {
            reset_client_cache();
            return do_get(key, val);
        }
        return rc;
    }

    int do_getRange(char* prefix, std::vector<wifs::KVPair>* batch_read) { 
        if (server_map.empty()){
            init_zk_connection(zk_server_ip);
            init_tmp_master();
        }
        auto it = server_map.lower_bound(somehashfunction(std::string(prefix)));
        if(it == server_map.end()) it = server_map.begin();
        if(options.wifsclient[it->second.serverid()] == NULL) init(it->second);
        int rc = options.wifsclient[it->second.serverid()]->wifs_GETRANGE(prefix, *batch_read);
        if(rc < 0) {
            reset_client_cache();
            return do_getRange(prefix, batch_read);
        }
        return rc;
    }

    int do_getRange_nilext(char* prefix){ 
        //just for performance testing
        if (server_map.empty()){
            init_zk_connection(zk_server_ip);
            init_tmp_master();
        }
        std::vector<wifs::KVPair> batch_read;
        auto it = server_map.lower_bound(somehashfunction(std::string(prefix)));
        if(it == server_map.end()) it = server_map.begin();
        if(options.wifsclient[it->second.serverid()] == NULL) init(it->second);
        int rc = options.wifsclient[it->second.serverid()]->wifs_GETRANGE(prefix, batch_read);
        if(rc==0) return batch_read.size();
        if(rc < 0) {
            reset_client_cache();
            return do_getRange_nilext(prefix);
        }
        return rc;
    }

    int do_put(char* key, char* val) {
        if (server_map.empty()){
            init_zk_connection(zk_server_ip);
            init_tmp_master();
        }
        auto it = server_map.lower_bound(somehashfunction(std::string(key)));
        if(it == server_map.end()) it = server_map.begin();
        if(options.wifsclient[it->second.serverid()] == NULL) init(it->second);
        int rc = options.wifsclient[it->second.serverid()]->wifs_PUT(key, val);
        if(rc < 0) {
            reset_client_cache();
            return do_put(key, val);
        }
        return rc;
    }

    int do_delete(char* key) {
        if (server_map.empty()){
            init_zk_connection(zk_server_ip);
            init_tmp_master();
        }
        auto it = server_map.lower_bound(somehashfunction(std::string(key)));
        if (it == server_map.end()) it = server_map.begin();
        if (options.wifsclient[it->second.serverid()] == NULL) init(it->second);
        int rc = options.wifsclient[it->second.serverid()]->wifs_DELETE(key);
        if(rc < 0) {
            reset_client_cache();
            return do_delete(key);
        }
        return rc;
    }
}

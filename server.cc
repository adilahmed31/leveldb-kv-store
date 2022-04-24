#include <bits/stdc++.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grpc/impl/codegen/status.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/status_code_enum.h>
#include <limits.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <csignal>

#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>
#include <algorithm>

#include "commonheaders.h"
#include "p2p.grpc.pb.h"
#include "wifs.grpc.pb.h"

#include <leveldb/cache.h>          
#include <leveldb/comparator.h>     
#include <leveldb/dumpfile.h>       
#include <leveldb/export.h>         
#include <leveldb/iterator.h>       
#include <leveldb/slice.h>          
#include <leveldb/table_builder.h>  
#include <leveldb/write_batch.h>    
#include <leveldb/c.h>              
#include <leveldb/db.h>             
#include <leveldb/env.h>            
#include <leveldb/filter_policy.h>  
#include <leveldb/options.h>        
#include <leveldb/status.h>         
#include <leveldb/table.h> 

#include "custom_fs.cc"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;
using grpc::StatusCode;

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;

using wifs::PutReq;
using wifs::PutRes;
using wifs::GetReq;
using wifs::GetRes;
using wifs::WIFS;

using p2p::HeartBeat;
using p2p::PeerToPeer;

char root_path[MAX_PATH_LENGTH];

int server_id = 0;
std::string this_node_address;
std::string cur_node_wifs_address;

std::vector<std::unique_ptr<PeerToPeer::Stub>> client_stub_(NUM_SERVERS);

leveldb::DB* db;

int get_dest_server_id(int key) {
    // compute the hash for the given key, find the corresponding server and return the id.
    // server id should range from [0, len(ip_server_wifs)), defined in commonheaders.h
    return key % 4;
}

void init_connection_with_peers() {
    for(int i = 0 ; i < NUM_SERVERS ; i++) {
        if(i == server_id) continue;
        client_stub_[i] = PeerToPeer::NewStub(grpc::CreateChannel(ip_servers_p2p[i], grpc::InsecureChannelCredentials()));
    }
}

void retry_connection_with_peer(int id) {
    client_stub_[id] = PeerToPeer::NewStub(grpc::CreateChannel(ip_servers_p2p[id], grpc::InsecureChannelCredentials()));
}

void killserver() {
    kill(getpid(), SIGKILL);
}

void mem_put(void) {
    
    return;
}

void flush(void) {
 
    return;
}

void get(void) {
    return;
}

class PeerToPeerServiceImplementation final : public PeerToPeer::Service {
    grpc::Status Ping(ServerContext* context, const p2p::HeartBeat* request, p2p::HeartBeat* reply) {
        std::cout << "Ping!" <<std::endl;
        return grpc::Status::OK;
    }

    grpc::Status p2p_PUT(ServerContext* context, const wifs::PutReq* request, wifs::PutRes* reply) override {
        std::cout<<"got put call from peer \n";
        leveldb::Status s = db->Put(leveldb::WriteOptions(), std::to_string(request->key()).c_str(), request->val().c_str());
        reply->set_status(s.ok() ? wifs::PutRes_Status_PASS : wifs::PutRes_Status_FAIL);
        return grpc::Status::OK;
    }

    grpc::Status p2p_GET(ServerContext* context, const wifs::GetReq* request, wifs::GetRes* reply) override {
        std::cout<<"got get call from peer \n";
        std::string val = "";
        leveldb::Status s = db->Get(leveldb::ReadOptions(), std::to_string(request->key()).c_str(), &val);
        reply->set_status(s.ok() ? wifs::GetRes_Status_PASS : wifs::GetRes_Status_FAIL);
        reply->set_val(val);
        return grpc::Status::OK;
    }

};

class WifsServiceImplementation final : public WIFS::Service {
    grpc::Status wifs_PUT(ServerContext* context, const wifs::PutReq* request, wifs::PutRes* reply) override {
        int dest_server_id = get_dest_server_id(request->key());
        if(dest_server_id != server_id) {
            std::cout<<"sending put to server "<<dest_server_id<<"\n";
            if(client_stub_[dest_server_id] == NULL) retry_connection_with_peer(dest_server_id);
            ClientContext context;
            grpc::Status status = client_stub_[dest_server_id]->p2p_PUT(&context, *request, reply);
            if(!status.ok()) {
                retry_connection_with_peer(dest_server_id);
                ClientContext context;
                status = client_stub_[dest_server_id]->p2p_PUT(&context, *request, reply);
            }
            reply->set_status(status.ok() ? wifs::PutRes_Status_PASS : wifs::PutRes_Status_FAIL);
            return grpc::Status::OK;
        }

        leveldb::Status s = db->Put(leveldb::WriteOptions(), std::to_string(request->key()).c_str(), request->val().c_str());
        reply->set_status(s.ok() ? wifs::PutRes_Status_PASS : wifs::PutRes_Status_FAIL);
        return grpc::Status::OK;
    }

    grpc::Status wifs_GET(ServerContext* context, const wifs::GetReq* request, wifs::GetRes* reply) override {
        int dest_server_id = get_dest_server_id(request->key());
        if(dest_server_id != server_id) {
            std::cout<<"sending get to server "<<dest_server_id<<"\n";
            if(client_stub_[dest_server_id] == NULL) retry_connection_with_peer(dest_server_id);
            ClientContext context;
            grpc::Status status = client_stub_[dest_server_id]->p2p_GET(&context, *request, reply);
            if(!status.ok()) {
                retry_connection_with_peer(dest_server_id);
                ClientContext context;
                status = client_stub_[dest_server_id]->p2p_GET(&context, *request, reply);
            }
            reply->set_status(status.ok() ? wifs::GetRes_Status_PASS : wifs::GetRes_Status_FAIL);
            return grpc::Status::OK;
        }

        std::string val = "";
        leveldb::Status s = db->Get(leveldb::ReadOptions(), std::to_string(request->key()).c_str(), &val);
        reply->set_status(s.ok() ? wifs::GetRes_Status_PASS : wifs::GetRes_Status_FAIL);
        reply->set_val(val);
        return grpc::Status::OK;
    }
};

void run_wifs_server() {
    WifsServiceImplementation service;
    ServerBuilder wifsServer;
    wifsServer.AddListeningPort(cur_node_wifs_address, grpc::InsecureServerCredentials());
    wifsServer.RegisterService(&service);
    std::unique_ptr<Server> server(wifsServer.BuildAndStart());
    std::cout << "WIFS Server listening on port: " << cur_node_wifs_address << std::endl;
    server->Wait();
}

void run_p2p_server() {
    PeerToPeerServiceImplementation service;
    ServerBuilder p2pServer;
    p2pServer.AddListeningPort(this_node_address, grpc::InsecureServerCredentials());
    p2pServer.RegisterService(&service);
    std::unique_ptr<Server> server(p2pServer.BuildAndStart());
    
    std::cout << "P2P Server listening on port: " << this_node_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {

    if (argc < 2) {
        std::cout << "Machine id not given\n";
        exit(1);
    }

    server_id = atoi(argv[1]);
    std::cout << "got machine id as " << server_id << "\n";
    
    this_node_address = ip_servers_p2p[server_id];
    cur_node_wifs_address = ip_server_wifs[server_id];

    // Create server path if it doesn't exist
    DIR* dir = opendir(getServerDir(server_id).c_str());
    if (ENOENT == errno) {
        mkdir(getServerDir(server_id).c_str(), 0777);
    }

    // spin off level db server locally
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Env* actual_env = leveldb::Env::Default();
    leveldb::Env* env = new CustomEnv(actual_env);
    options.env = env;

    leveldb::Status status = leveldb::DB::Open(options, getServerDir(server_id).c_str(), &db);
    assert(status.ok());


    init_connection_with_peers();
    std::thread p2p_server(run_p2p_server);
    run_wifs_server();

    return 0;
}

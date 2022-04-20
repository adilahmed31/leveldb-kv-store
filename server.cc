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
#include "p2p.grpc.p2p.h"
#include "wifs.grpc.p2p.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerPutr;
using grpc::Status;
using grpc::StatusCode;

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientGeter;

using wifs::PutReq;
using wifs::PutRes;
using wifs::GetReq;
using wifs::GetRes;

using p2p::HeartBeat;


char root_path[MAX_PATH_LENGTH];

int server_id = 0;
std::string this_node_address;
std::string cur_node_wifs_address;

std::unique_ptr<PeerToPeer::Stub> client_stub_;

void init_connection_with_peer(std::string peer_node_address) {
    client_stub_ = PeerToPeer::NewStub(grpc::CreateChannel(peer_node_address, grpc::InsecureChannelCredentials()));
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
    Status Ping(ServerContext* context, const HeartBeat* request, HeartBeat* reply) {
        std::cout << "Ping!" <<std::endl;
        return Status::OK;
    }
};

class WifsServiceImplementation final : public WIFS::Service {
    Status wifs_PUT(ServerContext* context, const PutReq* request,
                      PutRes* reply) override {
        int key = request->key();
        const auto value = std::to_string(request->val()).c_str();
        std::cout << "Put: Key - " << key << "Value - " << value <<std::endl;
        return Status::OK;
    }

    Status wifs_GET(ServerContext* context, const GetReq* request,
                     GetRes* reply) override {
        int key = request->key();
        std::cout << "Get: Key - " << key  <<std::endl;
        return Status::OK;
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
    sem_post(&sem_consensus);

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

    std::thread p2p_server(run_p2p_server);
    run_wifs_server();

    return 0;
}

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
#include <sys/time.h>
#include <unistd.h>
#include <csignal>
#include <chrono>
#include <ctime>
#include <condition_variable>
#include <typeinfo>

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
using wifs::ServerDetails;

using p2p::HeartBeat;
using p2p::PeerToPeer;
using p2p::ServerInit;
using p2p::StatusRes;
using p2p::SplitReq;

char root_path[MAX_PATH_LENGTH];

wifs::ServerDetails server_details;

int last_server_id = 0; //This is to be used only by first server currently.
int timestamp = 0; 

int ring_id = 0;
wifs::ServerDetails successor_server_details;

std::string this_node_address;
std::string cur_node_wifs_address;

std::vector<std::unique_ptr<PeerToPeer::Stub>> client_stub_(MAX_NUM_SERVERS);

auto then = std::chrono::system_clock::now();
std::condition_variable cv;

leveldb::DB* db;

void heartbeat(int heartbeat_server_id);

void broadcast_new_server_to_all(const p2p::ServerInit);

auto getServerIteratorInMap(int target_server_id) -> std::map<long,wifs::ServerDetails>::iterator{
    long hash_val = somehashfunction(std::to_string(target_server_id));
    auto it = server_map.find(hash_val);
    return it;
}

wifs::ServerDetails get_dest_server_details(std::string key) {
    // compute the hash for the given key, find the corresponding server and return the id.
    long key_hash = somehashfunction(key);
    std::cout << "key: " << key << ", hash: " << key_hash <<std::endl;
    auto it = server_map.lower_bound(key_hash);
    if(it==server_map.end()) it = server_map.begin();
    return it->second; //this should never happen 
}

//Return the successor of a node on the ring.
wifs::ServerDetails find_successor(int pred_server_id){
    int hash_val = somehashfunction(std::to_string(pred_server_id));
    auto it = server_map.upper_bound(hash_val);
    wifs::ServerDetails successor_id = (it == server_map.end() ? server_map.begin()->second : it->second);
    std::cout<<"Successor ID of server " << pred_server_id << " is: " << successor_id.serverid() << std::endl;
    return successor_id;
}

void connect_with_peer(wifs::ServerDetails server_details) {
    client_stub_[server_details.serverid()] = PeerToPeer::NewStub(grpc::CreateChannel(getP2PServerAddr(server_details.ipaddr(),server_details.serverid()), grpc::InsecureChannelCredentials()));
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

void populate_hash_server_map(google::protobuf::Map<long, wifs::ServerDetails>* map) {
    *map = google::protobuf::Map<long, wifs::ServerDetails>(server_map.begin(), server_map.end());
}

// Server IDs don't correspond to ring positions. Update ring position to a separate variable
void update_ring_id(){
    //Find Ring ID and successor ID
    auto it = getServerIteratorInMap(server_details.serverid());
    int ring_id = distance(server_map.begin(), it);
    auto it2 = std::next(it,1);
    successor_server_details = (it2 == server_map.end() ? server_map.begin()->second : it2->second);
    std::cout <<" Ring ID of this server : " <<ring_id <<" Successor server ID: "<<successor_server_details.serverid()<<std::endl;
}

//merges DB of server ID provided as an argument into the current node's DB
int merge_ldb(int failed_server_id){
    std::cout << "Merging DB of " <<failed_server_id << " into DB of node "<< server_details.serverid() <<std::endl;
    leveldb::Options options;
    leveldb::DB* db_merge;
    options.create_if_missing = false; //This should never be missing
    leveldb::Status s = leveldb::DB::Open(options, getServerDir(failed_server_id), &db_merge);

    if(!s.ok()){
        std::cout << "Error opening DB of failed node" <<std::endl;
        return -1;
    }
    leveldb::Iterator* iter = db_merge->NewIterator(leveldb::ReadOptions());

    leveldb::WriteOptions w;
    leveldb::WriteBatch writebatch;
    leveldb::WriteBatch deletebatch;

    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
    //write to batch
        writebatch.Put(iter->key(), iter->value());
        deletebatch.Delete(iter->key());
    }
    db->Write(w,&writebatch);
    std::cout << "Wrote entries from DB of server " <<failed_server_id<<std::endl;
    db_merge->Write(w,&deletebatch);
    std::cout << "Deleted all entries from DB of server "<<failed_server_id<<std::endl;
    delete db_merge;
    return 0;
}

std::string parse_ipmsg(std::string ipmsg){
    int start_index = ipmsg.find(":")+1;
    int end_index = ipmsg.find(":", start_index);
    return ipmsg.substr(start_index, end_index-start_index);
}

class PeerToPeerServiceImplementation final : public PeerToPeer::Service {
    grpc::Status Ping(ServerContext* context, const p2p::HeartBeat* request, p2p::HeartBeat* reply) {
        std::cout << "Ping!" <<std::endl;
        then = std::chrono::system_clock::now();
        return grpc::Status::OK;
    }

    grpc::Status AllotServerId(ServerContext* context, const p2p::HeartBeat* request, p2p::ServerInit* reply) {
        reply->set_id(++last_server_id);
        //get ip address
        std::string ipmsg = context->peer().c_str();
        reply->set_ipaddr(parse_ipmsg(ipmsg));
        std::cout <<"peer ip = " << reply->ipaddr() << std::endl;

        populate_hash_server_map(reply->mutable_servermap());
        return grpc::Status::OK;
    }

    grpc::Status InitializeNewServer(ServerContext* context, const p2p::ServerInit* request, p2p::HeartBeat* reply) {
        //Add to server list
        p2p::ServerInit request_copy = *request;
        insert_server_entry(request->id(), request->ipaddr());
        request_copy.set_action(p2p::ServerInit_Action_INSERT);
        broadcast_new_server_to_all(request_copy); //broadcast to the new server also, mode 0 for adding server ID
        if (client_stub_[request->id()] == NULL) {
            wifs::ServerDetails peerdetails;
            peerdetails.set_serverid(request->id());
            peerdetails.set_ipaddr(request->ipaddr());
            connect_with_peer(peerdetails);
        }
        std::thread hb(heartbeat, request->id());
        hb.detach();
        update_ring_id();
        print_ring();
        //heartbeat start
        
        return grpc::Status::OK;
    }

    grpc::Status BroadcastServerId(ServerContext* context, const p2p::ServerInit* request, p2p::HeartBeat* reply) {
        //Add new serverId to server list
        if(request->action() == p2p::ServerInit_Action_INSERT){
            insert_server_entry(request->id(),request->ipaddr());
            std::cout << "new server added: " << request->id() << std::endl;

        }
        else{
            remove_server_entry(request->id());
            std::cout << "server removed: " << request->id() << std::endl;

        }
        update_ring_id();
        print_ring();
        return grpc::Status::OK;
    }

    grpc::Status p2p_PUT(ServerContext* context, const wifs::PutReq* request, wifs::PutRes* reply) override {
        std::cout<<"got put call from peer \n";
        leveldb::WriteOptions write_options;
        write_options.sync = false;
        leveldb::Status s = db->Put(write_options, request->key().c_str(), request->val().c_str());
        reply->set_status(s.ok() ? wifs::PutRes_Status_PASS : wifs::PutRes_Status_FAIL);
        return grpc::Status::OK;
    }

    grpc::Status p2p_GET(ServerContext* context, const wifs::GetReq* request, wifs::GetRes* reply) override {
        std::cout<<"got get call from peer \n";
        std::string val = "";
        leveldb::Status s = db->Get(leveldb::ReadOptions(), request->key().c_str(), &val);
        reply->set_status(s.ok() ? wifs::GetRes_Status_PASS : wifs::GetRes_Status_FAIL);
        reply->set_val(val);
        return grpc::Status::OK;
    }

    //If this node is the predecessor of the newly joined node, commit the in-memory buffer to disk.
    //Don't need this with new implementation (TODO (Adil): Remove when current impl works)
    // grpc::Status CompactMemTable(ServerContext* context, const p2p::HeartBeat* request, p2p::StatusRes* reply){
    //     std::cout <<" predecessor asked to commit entries to disk" <<std::endl;
    //     leveldb::Status status = db->TEST_CompactMemTable(); //this relies on a patched version of levelDB (https://github.com/adilahmed31/leveldb)
    //     reply->set_status(status.ok() ? p2p::StatusRes_Status_PASS : p2p::StatusRes_Status_FAIL);
    //     return grpc::Status::OK;
    // }

    //When a new node joins the ring, it calls SplitDB on its successor. 
    //The successor iterates over the keys. When it sees a hash value matching the range sent in the 
    //RPC request, it writes it to the levelDB server of the calling node and deletes it from its own DB
    grpc::Status SplitDB(ServerContext* context, const p2p::SplitReq* request, p2p::StatusRes* reply){
        std::cout << "Server "<< request->id() <<" asked this server to split database" << std::endl;
        
        leveldb::Options options;
        // when the new server has just come up, though it has created the folder, it hasn't yet spun up 
        // a levelDB instance. we need to have this create flag for the writes to go through.
        options.create_if_missing = true;
        leveldb:WriteOptions w;
        leveldb::DB* db_split;
        leveldb::WriteBatch writebatch; //batched writes to new db
        leveldb::WriteBatch deletebatch; //batched deletes from old db
        //Iterator over DB of old node
        leveldb::Iterator* iter = db->NewIterator(ReadOptions());
        
        //Open new DB for joined node
        leveldb::Status s = leveldb::DB::Open(options, getServerDir(request->id()), &db_split);
        if(!s.ok()){
            std::cout << "Error opening DB of new node" <<std::endl;
            reply->set_status(p2p::StatusRes_Status_FAIL);
            return grpc::Status::OK;
        }
        
        //find range end for current server
        int this_range_end = getServerIteratorInMap(server_details.serverid())->first;
        
        //Iterate over old DB and create batches for operations
        for(iter->SeekToFirst(); iter->Valid(); iter->Next()){
            if ((somehashfunction(iter->key().ToString()) <= request->range_end()) && (somehashfunction(iter->key().ToString()) > this_range_end)){
                writebatch.Put(iter->key(), iter->value());
                deletebatch.Delete(iter->key());
            }
        }

        db_split->Write(w, &writebatch);
        db->Write(w, &deletebatch);
        delete db_split; //close new db so it can be re-opened by the calling server
        reply->set_status(p2p::StatusRes_Status_PASS);
        return grpc::Status::OK;
    }

    //When the master detects a failed server, it calls MergeDB on the successor of the failed node
    //The successor iterates over the failed node DB, reads in all its keys and adds it to a batched write
    //The batched write is applied to the successor's DB
    grpc::Status MergeDB(ServerContext* context, const p2p::ServerInit* request, p2p::StatusRes* reply){
        std::cout << "Master asked this server to merge with Server "<< request->id() <<std::endl;
        int rc = merge_ldb(request->id());
        if(rc < 0){
            std::cout << "merge_ldb returned -1. Error opening DB of failed node" <<std::endl;
            reply->set_status(p2p::StatusRes_Status_FAIL);
            return grpc::Status::OK;
        }
        reply->set_status(p2p::StatusRes_Status_PASS);
        return grpc::Status::OK;
    }
};

class WifsServiceImplementation final : public WIFS::Service {
    grpc::Status wifs_PUT(ServerContext* context, const wifs::PutReq* request, wifs::PutRes* reply) override {
        wifs::ServerDetails dest_server_details = get_dest_server_details(request->key());
        int dest_server_id = dest_server_details.serverid();
        if(dest_server_id != server_details.serverid()) {
            std::cout<<"sending put to server "<<dest_server_id<<"\n";
            if(client_stub_[dest_server_id] == NULL) connect_with_peer(dest_server_details);
            ClientContext context;
            grpc::Status status = client_stub_[dest_server_id]->p2p_PUT(&context, *request, reply);
            if(!status.ok()) {
                connect_with_peer(dest_server_details);
                ClientContext context;
                status = client_stub_[dest_server_id]->p2p_PUT(&context, *request, reply);
            }
            //else declare server failed - re assign keys
            reply->set_status(status.ok() ? wifs::PutRes_Status_PASS : wifs::PutRes_Status_FAIL);

            //populate the hash_server_map accordingly 
            populate_hash_server_map(reply->mutable_hash_server_map());

            return grpc::Status::OK;
        }
        leveldb::WriteOptions write_options;
        write_options.sync = false;
        leveldb::Status s = db->Put(write_options, request->key().c_str(), request->val().c_str());
        reply->set_status(s.ok() ? wifs::PutRes_Status_PASS : wifs::PutRes_Status_FAIL);
        populate_hash_server_map(reply->mutable_hash_server_map());
        return grpc::Status::OK;
    }

    grpc::Status wifs_GET(ServerContext* context, const wifs::GetReq* request, wifs::GetRes* reply) override {
        wifs::ServerDetails dest_server_details = get_dest_server_details(request->key());
        int dest_server_id = dest_server_details.serverid();
        if(dest_server_id != server_details.serverid()) {
            std::cout<<"sending get to server "<<dest_server_details.serverid()<<"\n";
            if(client_stub_[dest_server_id] == NULL) connect_with_peer(dest_server_details);
            ClientContext context;
            grpc::Status status = client_stub_[dest_server_id]->p2p_GET(&context, *request, reply);
            if(!status.ok()) {
                connect_with_peer(dest_server_details);
                ClientContext context;
                status = client_stub_[dest_server_id]->p2p_GET(&context, *request, reply);
            }
            //else declare server failed - re assign keys
            reply->set_status(status.ok() ? wifs::GetRes_Status_PASS : wifs::GetRes_Status_FAIL);

            //populate the hash_server_map accordingly 
            populate_hash_server_map(reply->mutable_hash_server_map());

            return grpc::Status::OK;
        }

        std::string val = "";
        leveldb::Status s = db->Get(leveldb::ReadOptions(), request->key().c_str(), &val);
        reply->set_status(s.ok() ? wifs::GetRes_Status_PASS : wifs::GetRes_Status_FAIL);
        reply->set_val(val);
        populate_hash_server_map(reply->mutable_hash_server_map());
        return grpc::Status::OK;
    }
};

//Call this function to broadcast any change in the server mappings. 
//mode 0 => insert server_id (node join), mode 1 => delete server_id (node exit)
void broadcast_new_server_to_all(const p2p::ServerInit idrequest){
  for(auto it = server_map.begin() ; it != server_map.end() ; it++) {
    if (it->second != MASTER_ID){ //don't broadcast to self (server 0)
        if(client_stub_[it->second.serverid()] == NULL) connect_with_peer(it->second);
        
        ClientContext context;
        p2p::HeartBeat hbreply;
        grpc::Status s = client_stub_[it->second.serverid()]->BroadcastServerId(&context, idrequest, &hbreply);
    }
  }
}

//Start listener for incoming client requests
void run_wifs_server() {
    WifsServiceImplementation service;
    ServerBuilder wifsServer;
    wifsServer.AddListeningPort(cur_node_wifs_address, grpc::InsecureServerCredentials());
    wifsServer.RegisterService(&service);
    std::unique_ptr<Server> server(wifsServer.BuildAndStart());
    std::cout << "WIFS Server listening on port: " << cur_node_wifs_address << std::endl;
    server->Wait();
}

//Start listener for incoming p2p requests (From other servers)
void run_p2p_server() {
    PeerToPeerServiceImplementation service;
    ServerBuilder p2pServer;
    p2pServer.AddListeningPort(this_node_address, grpc::InsecureServerCredentials());
    p2pServer.RegisterService(&service);
    std::unique_ptr<Server> server(p2pServer.BuildAndStart());
    
    std::cout << "P2P Server listening on port: " << this_node_address << std::endl;

    server->Wait();
}

void heartbeat(int heartbeat_server_id){

    // sleep here
    int retry_count = 0;
    while(retry_count < 5) {
        ClientContext context;
        p2p::HeartBeat hbrequest, hbreply;
        grpc::Status s = client_stub_[heartbeat_server_id]->Ping(&context, hbrequest, &hbreply);
        if(s.ok()) {
            retry_count = 0;
            std::cout<<heartbeat_server_id<<"'s HEART IS BEATING"<<std::endl;
        } else { 
            std::cout<<heartbeat_server_id<<"'s HEARTBEAT FAILED\n";
            retry_count++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    wifs::ServerDetails failed_server_successor_details = find_successor(heartbeat_server_id);
    int failed_server_successor_id = failed_server_successor_details.serverid();
    p2p::ServerInit mergerequest;
    mergerequest.set_id(heartbeat_server_id);  
       
    //if master is the successor of the failed node, perform the merge locally without an RPC call
    if(server_details.serverid() == failed_server_successor_id){
        std::cout << "Master itself is the successor of "<< heartbeat_server_id <<std::endl;
        merge_ldb(heartbeat_server_id);
    }
    //send a merge RPC to the successor of the failed node
    else{
        if (client_stub_[failed_server_successor_id] == NULL) connect_with_peer(failed_server_successor_details);

        std::cout << "failed_server_successor_id: " << failed_server_successor_id << std::endl;

        ClientContext context_merge;
        p2p::StatusRes mergereply;
        grpc::Status s = client_stub_[failed_server_successor_id]->MergeDB(&context_merge, mergerequest, &mergereply);
        std::cout << "s.ok()? " << s.ok() << " mergereply.status()" << mergereply.status() << std::endl;
        if ((!s.ok()) || (mergereply.status() == p2p::StatusRes_Status_FAIL)){
            std::cout << "Successor could not merge" <<std::endl; //TODO (Handle failure)
        }
    }
    remove_server_entry(heartbeat_server_id);

    //Update server maps of all servers to remove entry
    mergerequest.set_action(p2p::ServerInit_Action_DELETE);
    broadcast_new_server_to_all(mergerequest); //mode 1 is for deleting entries
    return;
            // TODO: figure out frequency of heartbeats, should we assume temporary failures and do retry  
}

void watch_for_master() {
    while(true && MASTER_ID != server_id) {
        auto now = std::chrono::system_clock::now();
        std::chrono::duration<double> diff = now - then;
        std::cout<<"Time elapsed between latest ping from master till now = "<<diff.count()<<std::endl;
        if(diff.count() > 5) {
            std::cout<<"----FINDING NEW MASTER----"<<std::endl;
            //TODO: can use to connect to master, kind of like how bigtable master does using Chubby. (figure out and handle locks maybe?)
             int next_master_id = INT_MAX;
             for(auto it = server_map.begin(); it != server_map.end(); it++) {
                    if(it->second != MASTER_ID) {
                        next_master_id = std::min(next_master_id, it->second);
                    }
             }
            MASTER_ID = next_master_id;
            std::cout<<"Next master id = "<<std::to_string(MASTER_ID)<<std::endl;
            std::ofstream file;
            file.open(master_file.c_str());
            file << std::to_string(MASTER_ID) << std::endl;
            then = std::chrono::system_clock::now();
            file.close();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
    }
}

void find_master_server() {
    master_file = getHomeDir() + "/.master"; 
    FILE *file = fopen(master_file.c_str(), "r");
    if (file) {
        fclose(file);
        std::ifstream t(master_file.c_str());
        std::stringstream buffer;
        buffer << t.rdbuf();
        
        MASTER_ID = stoi(buffer.str());
        // std::cout<<"INIT MASTER = "<<MASTER_ID<<std::endl;
    } else {
        MASTER_ID = 0;
    } 
    std::cout<<"----MASTER ID----"<<MASTER_ID<<std::endl;
}


void sigintHandler(int sig_num)
{
    std::cerr << "----CLEAN SHUTDOWN----\n";
    delete db;
    std::exit(0);

}

int main(int argc, char** argv) {
    //Ctrl + C handler
    signal(SIGINT, sigintHandler);
    /*
    Servers will be assigned (p2p,wifs) port numbers as (50060 + id, 50070+id), where id is incremented per server init.
    First server id = 0 and this is the server the client talks to, for now (master/load balancer + server). 
    First server maintains the list of servers and key ranges.
    When a new server (except first server) comes up, it will contact it's future successor and ask for transfer of keys. (flush)
    In the current scheme, the new server has to inform the first server (0) about it's presence.
    First server adds new server to it's list and redirects future requests. 
    */

    // Check if master is active or not. If file exists, then there's master, else, no master, so create file and become master
    find_master_server(); 
    //Check if firstserver exists
    server_details.set_serverid(MASTER_ID);
    server_details.set_ipaddr(MASTER_IP);
    connect_with_peer(server_details);

    ClientContext context_ping;
    p2p::HeartBeat hbrequest, hbreply;
    // TODO: not sure if needed, maybe useful later?
    int isMaster = 1;
    grpc::Status s = client_stub_[MASTER_ID]->Ping(&context_ping, hbrequest, &hbreply);
    std::cout << MASTER_ID << s.ok() << std::endl;
    
    if(s.ok()) {
        std::cout<<"Server initing, MASTER_ID =  "<<MASTER_ID<<std::endl;
        p2p::ServerId idreply;
        ClientContext context;
        s = client_stub_[MASTER_ID]->AllotServerId(&context, hbrequest, &idreply);
        server_id = idreply.id();
        
        // Create server path if it doesn't exist, should be done before calling split/merge db
        // and after getting server id from master.
        DIR* dir = opendir(getServerDir(server_id).c_str());
        if (ENOENT == errno) {
            mkdir(getServerDir(server_id).c_str(), 0777);
        }

        isMaster = 0;
        p2p::ServerInit idreply;
        ClientContext context;
        s = client_stub_[MASTER_ID]->AllotServerId(&context, hbrequest, &idreply);
        server_details.set_serverid(idreply.id());
        server_details.set_ipaddr(idreply.ipaddr());
        
        server_map = std::map<long, wifs::ServerDetails>(idreply.servermap().begin(),idreply.servermap().end());
        std::cout << "servermap initialized" << std::endl;
        update_ring_id(); //TODO: this function will be deprecated

        //Contact successor and transfer keys belonging to current node
        p2p::SplitReq splitrequest;
        p2p::StatusRes splitreply;

        splitrequest.set_range_end(somehashfunction(std::to_string(server_details.serverid())));
        ClientContext context_split;
        if(client_stub_[successor_server_details.serverid()] == NULL) connect_with_peer(successor_server_details);
        splitrequest.set_id(server_details.serverid());
        s = client_stub_[successor_server_details.serverid()]->SplitDB(&context_split, splitrequest, &splitreply);
        if (splitreply.status() == p2p::StatusRes_Status_FAIL){
            std::cout << "Successor could not sync" <<std::endl; //TODO (Handle failure)
        }
        print_ring();
    } else if (server_id == MASTER_ID) {
        // Create server path if it doesn't exist
        DIR* dir = opendir(getServerDir(server_id).c_str());
        if (ENOENT == errno) {
            mkdir(getServerDir(server_id).c_str(), 0777);
        }
        insert_server_entry(server_details.serverid(), server_details.ipaddr());
    }

    std::cout << "Set server id as " << server_details.serverid() << std::endl;
    this_node_address = getP2PServerAddr(server_details.ipaddr(), server_details.serverid());
    cur_node_wifs_address = getWifsServerAddr(server_details.ipaddr(), server_details.serverid());

    std::thread p2p_server(run_p2p_server);
    
    ClientContext context_init;
    if(!isMaster ) {
        s = client_stub_[MASTER_ID]->InitializeNewServer(&context_init, hbrequest, &hbreply);
        std::thread watch(watch_for_master);
        watch.detach();
    }

    // spin off level db server locally
    leveldb::Options options;
    options.create_if_missing = true;

    // now that we are doing consistent hashing and levelDB is being spun up on EFS, no need to use 
    // the custom storage interface, right?
    // leveldb::Env* actual_env = leveldb::Env::Default();
    // leveldb::Env* env = new CustomEnv(actual_env);
    // options.env = env;

    std::cout << getServerDir(server_details.serverid()) <<std::endl;
    leveldb::Status status = leveldb::DB::Open(options, getServerDir(server_details.serverid()).c_str(), &db);
    assert(status.ok());

    run_wifs_server();   
    return 0;
}

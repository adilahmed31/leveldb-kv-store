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
#include <dirent.h>

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

#include <conservator/ConservatorFrameworkFactory.h>
#include <conservator/ConservatorFramework.h>
#include <conservator/ExistsBuilder.h>
#include <check.h>

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
using p2p::ServerConfig;

std::string zk_server_addr = "127.0.0.1:2181";

char root_path[MAX_PATH_LENGTH];

wifs::ServerDetails server_details;

// changes to support write modes
p2p::ServerConfig config;

bool isMaster = false;
unique_ptr<ConservatorFramework> framework;
sem_t mutex_allot_server_id;

sem_t split_db_sem;
sem_t split_db_info_mutex;
int split_db_info = -1;

int timestamp = 0; 

int ring_id = 0;
wifs::ServerDetails successor_server_details;

std::string this_node_address;
std::string cur_node_wifs_address;

std::vector<std::unique_ptr<PeerToPeer::Stub>> client_stub_(MAX_NUM_SERVERS);

auto then = std::chrono::system_clock::now();
std::condition_variable cv;

leveldb::DB* db;
leveldb::DB* cache;

int do_cache = 1;

leveldb::WriteBatch global_write_batch;
int write_batch_counter = 0;
sem_t mutex_write_batch;

std::string server_config_str = "0,0,0";

void do_heartbeat(wifs::ServerDetails heartbeat_server_details);
void heartbeat_helper(wifs::ServerDetails heartbeat_server_details, int max_retries);
void heartbeat(wifs::ServerDetails heartbeat_server_id);

void broadcast_new_server_to_all(const p2p::ServerInit);

void check_and_release_split_db_sem(int id) {
    // release the split mutex if the new node failed after splitting
    sem_wait(&split_db_info_mutex);
    if(id == split_db_info) sem_post(&split_db_sem);
    split_db_info = -1;
    sem_post(&split_db_info_mutex);
}

auto getServerIteratorInMap(wifs::ServerDetails target_server_details) -> std::map<long,wifs::ServerDetails>::iterator{
    long hash_val = somehashfunction(getP2PServerAddr(target_server_details));
    auto it = server_map.find(hash_val);
    return it;
}

std::string filter_wildcard(std::string search_range){
    std::string delimiter = "*";
    std::string start_key = search_range.substr(0, search_range.find(delimiter));
    return start_key;
}


wifs::ServerDetails get_dest_server_details(std::string key) {
    // compute the hash for the given key, find the corresponding server and return the id.
    long key_hash = somehashfunction(key);
    // std::cout << "key: " << key << ", hash: " << key_hash <<std::endl;
    auto it = server_map.lower_bound(key_hash);
    if(it==server_map.end()) it = server_map.begin();
    return it->second; //this should never happen 
}

//Return the successor of a node on the ring.
wifs::ServerDetails find_successor(wifs::ServerDetails pred_server){
    int hash_val = somehashfunction(getP2PServerAddr(pred_server));
    auto it = server_map.upper_bound(hash_val);
    wifs::ServerDetails successor_id = (it == server_map.end() ? server_map.begin()->second : it->second);
    // std::cout<<"Successor ID of server " << pred_server.serverid() << " is: " << successor_id.serverid() << std::endl;
    return successor_id;
}

void connect_with_peer(wifs::ServerDetails server_details) {
    client_stub_[server_details.serverid()] = PeerToPeer::NewStub(grpc::CreateChannel(getP2PServerAddr(server_details), grpc::InsecureChannelCredentials()));
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

void populate_cur_node_server_details() {
    server_details.set_serverid(0);
    char arr[500];
    gethostname(arr, 500);
    server_details.set_ipaddr(arr);
}

void populate_hash_server_map(google::protobuf::Map<long, wifs::ServerDetails>* map) {
    *map = google::protobuf::Map<long, wifs::ServerDetails>(server_map.begin(), server_map.end());
}

// Server IDs don't correspond to ring positions. Update ring position to a separate variable
void update_ring_id(){
    //Find Ring ID and successor ID
    auto it = getServerIteratorInMap(server_details);
    ring_id = distance(server_map.begin(), it);
    auto it2 = std::next(it,1);
    successor_server_details = (it2 == server_map.end() ? server_map.begin()->second : it2->second);
    // std::cout <<" Ring ID of this server : " <<ring_id <<" Successor server ID: "<<successor_server_details.serverid()<<std::endl;
}

//merges DB of server ID provided as an argument into the current node's DB
int merge_ldb(wifs::ServerDetails failed_server_details){
    check_and_release_split_db_sem(failed_server_details.serverid());

    int failed_server_id = failed_server_details.serverid();
    // std::cout << "Merging DB of " <<failed_server_id << " into DB of node "<< server_details.serverid() <<std::endl;
    leveldb::Options options;
    leveldb::DB* db_merge;
    options.create_if_missing = false; //This should never be missing
    // std::cout << "Trying to merge with DB at path :" << getServerDir(failed_server_id) << std::endl;
    leveldb::Status s = leveldb::DB::Open(options, getServerDir(failed_server_id), &db_merge);

    if(!s.ok()){
        std::cout << "Error opening DB of failed node" <<std::endl;
        leveldb::Status status = leveldb::DestroyDB(getServerDir(failed_server_id), leveldb::Options());
        try{
            std::experimental::filesystem::remove_all(getServerDir(failed_server_id));
        }
        catch(const std::exception& e){
            // std::cout << "Could not delete Server directory : " << getServerDir(failed_server_id) <<std::endl;
        }
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
    if(do_cache) cache->Write(w,&writebatch);
    // std::cout << "Wrote entries from DB of server " <<failed_server_id<<std::endl;
    db_merge->Write(w,&deletebatch);
    // std::cout << "Deleted all entries from DB of server "<<failed_server_id<<std::endl;
    std::cout << "merged db of " <<failed_server_id << " into the current server\n";
    delete db_merge;
    leveldb::Status status = leveldb::DestroyDB(getServerDir(failed_server_id), leveldb::Options());
    try{
         std::experimental::filesystem::remove_all(getServerDir(failed_server_id));
    }
    catch(const std::exception& e) {
        // std::cout <<" Could not delete server directory " << getServerDir(failed_server_id) << std::endl;
    }
    return 0;
}

std::string parse_ipmsg(std::string ipmsg){
    int start_index = ipmsg.find(":") + 1;
    int end_index = ipmsg.find(":", start_index);
    return ipmsg.substr(start_index, end_index-start_index);
}

// helper function to execute local put call.
void execute_local_put(const wifs::PutReq* request, wifs::PutRes* reply) {
    leveldb::WriteOptions write_options;
    write_options.sync = false;
    
    int batch_size = config.mode() == p2p::ServerConfig_Mode_WRITE ? config.num_batch() : 1;
    
    sem_wait(&mutex_write_batch); // acquire the lock
    global_write_batch.Put(request->key(), request->val().c_str());
    // std::cout << "request->key() " << request->key() << "request->val().c_str()" << request->val().c_str() << std::endl;
    if(write_batch_counter >= batch_size - 1) {
        if(do_cache){
            leveldb::Status s_cache = cache->Write(write_options, &global_write_batch);
            do_cache = s_cache.ok() ? 1 : 0;
        }
        leveldb::Status s_remote = db->Write(write_options, &global_write_batch);
        global_write_batch.Clear(); // release the lock
        write_batch_counter = 0;
        // std::cout << "s_remote.ok() " << s_remote.ok() << std::endl;
        reply->set_status(s_remote.ok() ? wifs::PutRes_Status_PASS : wifs::PutRes_Status_FAIL);
    } else {
        write_batch_counter++;
        // std::cout << "write batch count " << write_batch_counter << std::endl;
        reply->set_status(wifs::PutRes_Status_PASS);
    }
    sem_post(&mutex_write_batch);
}

void update_pending_writes() {
    leveldb::WriteOptions write_options;
    sem_wait(&mutex_write_batch);
    if(do_cache){
        // std::cout<<"Caching enabled!"<<std::endl;
        cache->Write(write_options, &global_write_batch);
    }
    db->Write(write_options, &global_write_batch);
    global_write_batch.Clear();
    write_batch_counter = 0;
    sem_post(&mutex_write_batch);
}

void get_as_per_mode(const wifs::GetReq* request, wifs::GetRes* reply){
   leveldb::Status s;
   if (request->mode() == 1){
        leveldb::ReadOptions options;
        leveldb::Iterator* it;
        options.fill_cache = false;
        if (do_cache){
            it = cache->NewIterator(options);
        }
        else{
            it = db->NewIterator(options);
        }
        std::string sk = filter_wildcard(request->key().c_str());
        leveldb::Slice start_key = sk;
        for (it->Seek(start_key); it->Valid(); it->Next()) {
            if(it->key().ToString().substr(0,config.prefix_length()) != sk ) break;
            // std::cout << "Found key: " << it->key().ToString() << std::endl;
            wifs::KVPair* kv_pair = reply->add_kvpairs();
            kv_pair->set_key(it->key().ToString());
            kv_pair->set_value(it->value().ToString());
            // std::cout << "kv pairs size: " << reply->kvpairs_size() << std::endl;
        }
        reply->set_status(wifs::GetRes_Status_PASS); 
    }
    else{
        std::string val = "";
        if (do_cache){
            s = cache->Get(leveldb::ReadOptions(), request->key().c_str(), &val);
            if(!s.ok()){
                s = db->Get(leveldb::ReadOptions(), request->key().c_str(), &val);
                do_cache = 0;
            }
        }
        else{
            s = db->Get(leveldb::ReadOptions(), request->key().c_str(), &val);
        }
        reply->set_status(s.ok() ? wifs::GetRes_Status_PASS : wifs::GetRes_Status_FAIL);
        reply->set_val(val);
    }
}

class PeerToPeerServiceImplementation final : public PeerToPeer::Service {
    grpc::Status Ping(ServerContext* context, const p2p::HeartBeat* request, p2p::HeartBeat* reply) {
        // std::cout << "Ping!" <<std::endl;
        then = std::chrono::system_clock::now();
        return grpc::Status::OK;
    }

    grpc::Status PingMaster(ServerContext* context, const p2p::ServerInit* request, p2p::HeartBeat* reply) {
        std::cout << "new server with id "<<request->id()<<" trying to join ring"<<std::endl;
        then = std::chrono::system_clock::now();
        wifs::ServerDetails hb_server_details;
        hb_server_details.set_serverid(request->id());
        hb_server_details.set_ipaddr(request->ipaddr());
        do_heartbeat(hb_server_details);
        return grpc::Status::OK;
    }

    grpc::Status AllotServerId(ServerContext* context, const p2p::HeartBeat* request, p2p::ServerInit* reply) {
        sem_wait(&mutex_allot_server_id);
        int next_poss_server_id = atoi(framework->getData()->forPath("/next_poss_server_id").c_str());
        framework->deleteNode()->deletingChildren()->forPath("/next_poss_server_id");
        framework->create()->forPath("/next_poss_server_id", (char *) std::to_string(next_poss_server_id + 1).c_str());
        sem_post(&mutex_allot_server_id);

        reply->set_id(next_poss_server_id);
        std::cout <<"new server request, alloting id = " << next_poss_server_id << std::endl;

        populate_hash_server_map(reply->mutable_servermap());
        return grpc::Status::OK;
    }

    grpc::Status InitializeNewServer(ServerContext* context, const p2p::ServerInit* request, p2p::HeartBeat* reply) {
        //Add to server list
        // std::cout<<"HELLOOOO"<<std::endl;
        p2p::ServerInit request_copy = *request;
        
        wifs::ServerDetails sd;
        sd.set_serverid(request->id());
        sd.set_ipaddr(request->ipaddr());
        insert_server_entry(sd);
        
        request_copy.set_action(p2p::ServerInit_Action_INSERT);
        std::cout<<"new server added: "<< request->id() <<"\n";
        broadcast_new_server_to_all(request_copy); //broadcast to the new server also, mode 0 for adding server ID
        
        // no need to start heartbeat here, as it is being started in PingMaster itself. 
        
        update_ring_id();
        print_ring();
        return grpc::Status::OK;
    }

    grpc::Status BroadcastServerId(ServerContext* context, const p2p::ServerInit* request, p2p::HeartBeat* reply) {
        //Add new serverId to server list
        wifs::ServerDetails sd;
        sd.set_serverid(request->id());
        sd.set_ipaddr(request->ipaddr());
        if(request->action() == p2p::ServerInit_Action_INSERT){
            insert_server_entry(sd);
            std::cout << "new server added: " << request->id() << std::endl;
            check_and_release_split_db_sem(request->id());
        }
        else{
            remove_server_entry(sd);
            std::cout << "server removed: " << request->id() << std::endl;
        }
        update_ring_id();
        print_ring();
        return grpc::Status::OK;
    }

    grpc::Status p2p_PUT(ServerContext* context, const wifs::PutReq* request, wifs::PutRes* reply) override {
        std::cout<<"got PUT call from peer for key "<< request->key() <<"\n";
        execute_local_put(request, reply);
        return grpc::Status::OK;
    }

    grpc::Status p2p_GET(ServerContext* context, const wifs::GetReq* request, wifs::GetRes* reply) override {
        std::cout<<"got GET call from peer for key " << request->key() <<"\n";
        update_pending_writes();
        get_as_per_mode(request, reply);
        return grpc::Status::OK;
    }

    grpc::Status p2p_DELETE(ServerContext* context, const wifs::DeleteReq* request, wifs::DeleteRes* reply) override {
        // std::cout<<"got delete call from peer \n";

        update_pending_writes();

        leveldb::Status s = db->Delete(leveldb::WriteOptions(), request->key().c_str());
        reply->set_status(s.ok() ? wifs::DeleteRes_Status_PASS : wifs::DeleteRes_Status_FAIL);
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
        // std::cout << "Server "<< request->id() <<" asked this server to split database" << std::endl;
        
        // only 1 split shuld happen at a time
        sem_wait(&split_db_sem);
        sem_wait(&split_db_info_mutex);
        split_db_info = request->id();
        sem_post(&split_db_info_mutex);


        // check again if you are the latest successor.
        wifs::ServerDetails caller_server_details;
        caller_server_details.set_ipaddr(request->ipaddr());
        caller_server_details.set_serverid(request->id());
        wifs::ServerDetails new_successor_sd = find_successor(caller_server_details);
        if(new_successor_sd.serverid() != server_details.serverid()) {
            ClientContext context_split;
            if(client_stub_[new_successor_sd.serverid()] == NULL) connect_with_peer(new_successor_sd);
            return client_stub_[new_successor_sd.serverid()]->SplitDB(&context_split, *request, reply);
        }
        

        leveldb::Options options;
        // when the new server has just come up, though it has created the folder, it hasn't yet spun up 
        // a levelDB instance. we need to have this create flag for the writes to go through.
        options.create_if_missing = true;
        leveldb:WriteOptions w;
        leveldb::DB* db_split;
        leveldb::WriteBatch writebatch; //batched writes to new db
        leveldb::WriteBatch deletebatch; //batched deletes from old db
        //Iterator over DB of old node
        leveldb::Iterator* iter;
        if(do_cache) iter = cache->NewIterator(ReadOptions());
        else iter = db->NewIterator(ReadOptions());
        
        //Open new DB for joined node
        leveldb::Status s = leveldb::DB::Open(options, getServerDir(request->id()), &db_split);
        if(!s.ok()){
            std::cout << "Error opening DB of new node" <<std::endl;
            reply->set_status(p2p::StatusRes_Status_FAIL);
            return grpc::Status::OK;
        }
        
        //find range end for current server
        int this_range_end = getServerIteratorInMap(server_details)->first;
        
        //Iterate over old DB and create batches for operations
        for(iter->SeekToFirst(); iter->Valid(); iter->Next()){
            int cur_key_hash = somehashfunction(iter->key().ToString());
            if(request->range_end() < this_range_end) {
                if (cur_key_hash <= request->range_end() || cur_key_hash > this_range_end){
                    writebatch.Put(iter->key(), iter->value());
                    deletebatch.Delete(iter->key());
                }
            }
            else {
                if (cur_key_hash > this_range_end && cur_key_hash <= request->range_end()){
                    writebatch.Put(iter->key(), iter->value());
                    deletebatch.Delete(iter->key());
                }
            }
        }

        db_split->Write(w, &writebatch);
        db->Write(w, &deletebatch);
        if(do_cache) cache->Write(w,&deletebatch);
        delete db_split; //close new db so it can be re-opened by the calling server
        reply->set_status(p2p::StatusRes_Status_PASS);
        std::cout << "splitting db with server "<< request->id() << std::endl;
        return grpc::Status::OK;
    }

    //When the master detects a failed server, it calls MergeDB on the successor of the failed node
    //The successor iterates over the failed node DB, reads in all its keys and adds it to a batched write
    //The batched write is applied to the successor's DB
    grpc::Status MergeDB(ServerContext* context, const p2p::ServerInit* request, p2p::StatusRes* reply){
        // std::cout << "Master asked this server to merge with Server "<< request->id() <<std::endl;

        wifs::ServerDetails sd;
        sd.set_serverid(request->id());
        sd.set_ipaddr(request->ipaddr());
        
        int rc = merge_ldb(sd);
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
        std::string hash_key = config.mode() == p2p::ServerConfig_Mode_READ ? 
                request->key().substr(0, std::min((int) config.prefix_length(), (int) (request->key().length()))) : 
                request->key();

        wifs::ServerDetails dest_server_details = get_dest_server_details(hash_key);
        int dest_server_id = dest_server_details.serverid();
        if(dest_server_id != server_details.serverid()) {
            std::cout<<"redirecting put request to "<<dest_server_id<<"\n";
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
        std::cout<<"got PUT call from client for key "<<request->key()<<"\n";
        execute_local_put(request, reply);
        populate_hash_server_map(reply->mutable_hash_server_map());
        return grpc::Status::OK;
    }

    grpc::Status wifs_GET(ServerContext* context, const wifs::GetReq* request, wifs::GetRes* reply) override {
        std::string hash_key = config.mode() == p2p::ServerConfig_Mode_READ ? 
                request->key().substr(0, std::min((int) config.prefix_length(), (int) (request->key().length()))) : 
                request->key();

        wifs::ServerDetails dest_server_details = get_dest_server_details(hash_key);
        
        int dest_server_id = dest_server_details.serverid();
        if(dest_server_id != server_details.serverid()) {
            std::cout<<"redirecting get request to "<<dest_server_details.serverid()<<"\n";
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
        std::cout<<"got GET call from client for key "<<request->key()<<"\n";
        update_pending_writes();
        get_as_per_mode(request, reply);
        populate_hash_server_map(reply->mutable_hash_server_map());
        return grpc::Status::OK;
    }

    grpc::Status wifs_DELETE(ServerContext* context, const wifs::DeleteReq* request, wifs::DeleteRes* reply) override {
        std::string hash_key = config.mode() == p2p::ServerConfig_Mode_READ ? 
                request->key().substr(0, std::min((int) config.prefix_length(), (int) (request->key().length()))) : 
                request->key();

        wifs::ServerDetails dest_server_details = get_dest_server_details(hash_key);
        int dest_server_id = dest_server_details.serverid();
        if(dest_server_id != server_details.serverid()) {
            // std::cout<<"sending delete to server "<<dest_server_details.serverid()<<"\n";
            if(client_stub_[dest_server_id] == NULL) connect_with_peer(dest_server_details);
            ClientContext context;
            grpc::Status status = client_stub_[dest_server_id]->p2p_DELETE(&context, *request, reply);
            if(!status.ok()) {
                connect_with_peer(dest_server_details);
                ClientContext context;
                status = client_stub_[dest_server_id]->p2p_DELETE(&context, *request, reply);
            }
            //else declare server failed - re assign keys
            reply->set_status(status.ok() ? wifs::DeleteRes_Status_PASS : wifs::DeleteRes_Status_FAIL);

            //populate the hash_server_map accordingly 
            populate_hash_server_map(reply->mutable_hash_server_map());

            return grpc::Status::OK;
        }

        update_pending_writes();

        leveldb::Status s = db->Delete(leveldb::WriteOptions(), request->key().c_str());
        reply->set_status(s.ok() ? wifs::DeleteRes_Status_PASS : wifs::DeleteRes_Status_FAIL);
        populate_hash_server_map(reply->mutable_hash_server_map());
        return grpc::Status::OK;
    }
};

//Call this function to broadcast any change in the server mappings. 
//mode 0 => insert server_id (node join), mode 1 => delete server_id (node exit)
void broadcast_new_server_to_all(const p2p::ServerInit idrequest){
  for(auto it = server_map.begin() ; it != server_map.end() ; it++) {
    if (it->second.serverid() != MASTER_ID) { //don't broadcast to self (server 0)
        if(client_stub_[it->second.serverid()] == NULL) connect_with_peer(it->second);
        
        ClientContext context;
        p2p::HeartBeat hbreply;
        grpc::Status s = client_stub_[it->second.serverid()]->BroadcastServerId(&context, idrequest, &hbreply);
    } else check_and_release_split_db_sem(idrequest.id());
  }
}

//Start listener for incoming client requests
void run_wifs_server() {
    WifsServiceImplementation service;
    ServerBuilder wifsServer;
    wifsServer.AddListeningPort(cur_node_wifs_address, grpc::InsecureServerCredentials());
    wifsServer.RegisterService(&service);
    std::unique_ptr<Server> server(wifsServer.BuildAndStart());
    std::cout << "running external server on " << cur_node_wifs_address << std::endl;
    server->Wait();
}

//Start listener for incoming p2p requests (From other servers)
void run_p2p_server() {
    PeerToPeerServiceImplementation service;
    ServerBuilder p2pServer;
    p2pServer.AddListeningPort(this_node_address, grpc::InsecureServerCredentials());
    p2pServer.RegisterService(&service);
    std::unique_ptr<Server> server(p2pServer.BuildAndStart());
    
    std::cout << "running p2p server on " << this_node_address << std::endl;

    server->Wait();
}

void heartbeat_helper(wifs::ServerDetails hb_server_details, int max_retries) {
    if (client_stub_[hb_server_details.serverid()] == NULL) connect_with_peer(hb_server_details);
    int retry_count = 0;
    while(retry_count < max_retries) {
        ClientContext context;
        p2p::HeartBeat hbrequest, hbreply;
        grpc::Status s = client_stub_[hb_server_details.serverid()]->Ping(&context, hbrequest, &hbreply);
        if(s.ok()) {
            retry_count = 0;
            printf(".");
            fflush(stdout);
        } else { 
            // std::cout<<hb_server_details.serverid()<<"'s HEARTBEAT FAILED\n";
            retry_count++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

p2p::ServerInit merge_db_helper(wifs::ServerDetails heartbeat_server_details) {
    wifs::ServerDetails failed_server_successor_details = find_successor(heartbeat_server_details);
    int failed_server_successor_id = failed_server_successor_details.serverid();
    p2p::ServerInit mergerequest;
    mergerequest.set_id(heartbeat_server_details.serverid());  
    mergerequest.set_ipaddr(heartbeat_server_details.ipaddr());
    std::cout<<"initiating merge of "<<heartbeat_server_details.serverid()<<"'s db with successor "<<failed_server_successor_id<<"\n";
    //if master is the successor of the failed node, perform the merge locally without an RPC call
    if(server_details.serverid() == failed_server_successor_id){
        // std::cout << "Master itself is the successor of "<< heartbeat_server_details.serverid() <<std::endl;
        merge_ldb(heartbeat_server_details);
    }
    //send a merge RPC to the successor of the failed node
    else{
        if (client_stub_[failed_server_successor_id] == NULL) connect_with_peer(failed_server_successor_details);

        // std::cout << "failed_server_successor_id: " << failed_server_successor_id << std::endl;

        ClientContext context_merge;
        p2p::StatusRes mergereply;
        grpc::Status s = client_stub_[failed_server_successor_id]->MergeDB(&context_merge, mergerequest, &mergereply);
        // std::cout << "s.ok()? " << s.ok() << " mergereply.status()" << mergereply.status() << std::endl;
        if ((!s.ok()) || (mergereply.status() == p2p::StatusRes_Status_FAIL)){
            std::cout << "successor could not merge" <<std::endl; //TODO (Handle failure)
        }
    }
    return mergerequest;
}

void heartbeat(wifs::ServerDetails hb_server_details){
    heartbeat_helper(hb_server_details, 5);
    std::cout<<"server "<<hb_server_details.serverid()<<" is unreachable\n";
    // heartbeat failed after 5 retries, so now merge DBs
    p2p::ServerInit mergerequest = merge_db_helper(hb_server_details);
    // remove failed server from server_map
    remove_server_entry(hb_server_details);
    //Update server maps of all servers to remove entry
    mergerequest.set_action(p2p::ServerInit_Action_DELETE);
    broadcast_new_server_to_all(mergerequest); //mode 1 is for deleting entries
    return;
    // TODO: figure out frequency of heartbeats, should we assume temporary failures and do retry  
}

void do_heartbeat(wifs::ServerDetails hb_server_details) {
    std::thread hb(heartbeat, hb_server_details);
    hb.detach();
}

void find_master_server() {
    if(framework->checkExists()->forPath("/config") == ZOK) {
        server_config_str = framework->getData()->forPath("/config");
        // std::cout<<"currently running server under config "<<server_config_str<<"\n";
    }
    int ret = framework->create()->withFlags(ZOO_EPHEMERAL)->forPath("/master", getP2PServerAddr(server_details).c_str());
    if (ret == ZNODEEXISTS) {
        // file exists
        isMaster = false;
        std::string master_server_details_str = framework->getData()->forPath("/master");
        int delim_pos = (int) master_server_details_str.find(":");
        MASTER_IP = master_server_details_str.substr(0, delim_pos);
        MASTER_ID = atoi(master_server_details_str.substr(delim_pos+1, 5).c_str()) - 50060;
        // std::cout<<"----MASTER_ID----"<<MASTER_ID<<std::endl;
        std::cout<<"master ip read from zookeeper "<<MASTER_IP<<std::endl;
        return;
    }

    isMaster = true;
    MASTER_ID = server_details.serverid();
    MASTER_IP = server_details.ipaddr();
    
    std::cout<<"no current master, becoming one!!!\n";
    // will go through only the first time
    ret = framework->create()->forPath("/next_poss_server_id", (char *) "1");
    // if (ret == ZNODEEXISTS) {
    //     std::cout<<"next_poss_server_id exists, dind't overwrite\n";
    // } else std::cout<<"creating next_poss_server_id\n";
}

void watch_for_master() {
    while(MASTER_ID != server_details.serverid()) {
        auto now = std::chrono::system_clock::now();
        std::chrono::duration<double> diff = now - then;
        printf(",");
        fflush(stdout);
        if(diff.count() > 3) {
            // std::cout<<"----FINDING NEW MASTER----"<<std::endl;
            std::cout<<"finding new master..."<<std::endl;
            find_master_server();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
    }
    // now that you are the master start heartbeats with everybody
    for(auto it = server_map.begin() ; it != server_map.end() ; it++) {
        if(it->second.serverid() == server_details.serverid()) continue;
        do_heartbeat(it->second);
    }
}

void sigintHandler(int sig_num)
{
    std::cerr << "sigkill issued...\n";
    update_pending_writes();
    if(isMaster) {
        // delete master file in zk
        framework->deleteNode()->deletingChildren()->forPath("/master");
    }
    framework->close();
    if(do_cache){
        delete cache;
        leveldb::Status status = leveldb::DestroyDB(getCacheDir(server_details.serverid()), leveldb::Options());
        try{
            std::experimental::filesystem::remove_all(getCacheDir(server_details.serverid()));
        }
        catch(const std::exception& e){
            // std::cout << "Could not delete Cache directory : " << getCacheDir(server_details.serverid()) <<std::endl;
        }
        
    }
    delete db;
    kill(getpid(), SIGTERM);
}

void init_p2p_server() {
    // std::cout << "Set server id as " << server_details.serverid() << std::endl;
    this_node_address = getP2PServerAddr(server_details);
    std::thread p2p_server(run_p2p_server);
    p2p_server.detach();
}

void init_zk_connection() {
    // clientid_t zk_client;
    // zk_client.client_id = client_id;
    // strcpy(zk_client.passwd, "lol");

    ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
    framework = factory.newClient(zk_server_addr.c_str(),1000);
    framework->start();
}

p2p::ServerInit get_cur_server_details_and_initialize_map(){
    p2p::ServerInit idreply;
    wifs::ServerDetails master_server_details;
    master_server_details.set_ipaddr(MASTER_IP);
    master_server_details.set_serverid(MASTER_ID);
    connect_with_peer(master_server_details);
    // std::cout<<"Server initing, MASTER_ID =  "<<MASTER_ID<<std::endl;
    isMaster = false;
    ClientContext context;
    p2p::HeartBeat hbrequest;
    grpc::Status s = client_stub_[MASTER_ID]->AllotServerId(&context, hbrequest, &idreply);
    server_details.set_serverid(idreply.id());
    idreply.set_ipaddr(server_details.ipaddr());

    server_map = std::map<long, wifs::ServerDetails>(idreply.servermap().begin(),idreply.servermap().end());
    // std::cout << "servermap initialized" << std::endl;
    return idreply;
}

void init_server_dir(){
    DIR* dir = opendir(getServerDir(server_details.serverid()).c_str());
    if (ENOENT == errno) {
        mkdir(getServerDir(server_details.serverid()).c_str(), 0777);
    }
    closedir(dir);
}

void init_cache_dir(){
    std::string server_dir = getServerDir(server_details.serverid());
    std::string cache_dir = getCacheDir(server_details.serverid());
    std::experimental::filesystem::remove_all(cache_dir);
    std::experimental::filesystem::copy(server_dir, cache_dir,  std::experimental::filesystem::copy_options::recursive);
}

void ping_master_wrapper(p2p::ServerInit idreply){
    ClientContext context1;
    p2p::HeartBeat hbreply1;
    grpc::Status s1 = client_stub_[MASTER_ID]->PingMaster(&context1, idreply, &hbreply1);
}

void split_db_wrapper(){
    ClientContext context_split;
    p2p::SplitReq splitrequest;
    p2p::StatusRes splitreply;

    splitrequest.set_range_end(somehashfunction(getP2PServerAddr(server_details)));
    if(client_stub_[successor_server_details.serverid()] == NULL) connect_with_peer(successor_server_details);
    splitrequest.set_id(server_details.serverid());
    splitrequest.set_ipaddr(server_details.ipaddr());
    grpc::Status s = client_stub_[successor_server_details.serverid()]->SplitDB(&context_split, splitrequest, &splitreply);
    if (splitreply.status() == p2p::StatusRes_Status_FAIL){
        std::cout << "Successor could not sync" <<std::endl; //TODO (Handle failure)
    }
}

void init_server_and_watch_master(p2p::ServerInit idreply){
    ClientContext context_init;
    p2p::HeartBeat hbreply;
    grpc::Status s = client_stub_[MASTER_ID]->InitializeNewServer(&context_init, idreply, &hbreply);
    std::thread watch(watch_for_master);
    watch.detach();
}

std::vector<std::string> parse_string(std::string s, std::string delimiter) {
    std::vector<std::string> ans;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        ans.push_back(token);
        // std::cout<<token<<"\n";
        s.erase(0, pos + delimiter.length());
    }
    ans.push_back(s);
    return ans;
}


/* Set mode based on config. Modes can be:
DEFAULT = 0;
WRITE = 1; //batch writes at server before committing
READ = 2; ////enable locality for storing keys
*/

void get_config() {
    std::vector<std::string> values = parse_string(server_config_str, ",");
    
    config.set_mode(static_cast<p2p::ServerConfig_Mode>(stoi(values[0])));
    config.set_num_batch(stoi(values[1]));
    config.set_prefix_length(stoi(values[2]));
    // std::cout << config.mode() <<"  " <<config.num_batch() << "  " <<config.prefix_length() << std::endl;
    //Update caching based on value of config
    // do_cache = config.mode() == p2p::ServerConfig_Mode_WRITE ? 0 : 1;
}

void collect_db() {
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (getHomeDir().c_str())) != NULL) {
        // std::cout << dir << std::endl;
        while ((ent = readdir(dir)) != NULL) {
            std::string serverdb = ent->d_name;
            if (serverdb.rfind(".server", 0) ==0){
                // std::cout << serverdb.substr(7) << std::endl;
                wifs::ServerDetails sd;
                // std::string serverid = serverdb.back();
                sd.set_serverid(std::stoi(serverdb.substr(7))); //hardcoded value of 7 as that's the index at which .server ends
                //not setting ip address - is that ok?
                if (sd.serverid()!=0) merge_ldb(sd);
                //not checking rc
            }
        }
        closedir (dir);
    }
}

int main(int argc, char** argv) {
    zoo_set_debug_level((ZooLogLevel)0);
    // Usage './server <zk_server_address> <efs_mount_path>'
    // efs_mount_path shouldn't have a trailing /
    // Default values : zk_server_address - 127.0.0.1:2181
    //                  efs_mount_path - ~

    /*
    Servers will be assigned (p2p,wifs) port numbers as (50060 + id, 50170+id), where id is incremented per server init.
    First server id = 0 and this is the server the client talks to, for now (master/load balancer + server). 
    First server maintains the list of servers and key ranges.
    When a new server (except first server) comes up, it will contact it's future successor and ask for transfer of keys. (flush)
    In the current scheme, the new server has to inform the first server (0) about it's presence.
    First server adds new server to it's list and redirects future requests. 
    */

    if(argc > 1) {
        // overwrite zk server address with the passed in value
        zk_server_addr = std::string(argv[1]);
    }

    if(argc > 2) {
        efs_mount_path = std::string(argv[2]);
    }

    //Ctrl + C handler
    signal(SIGINT, sigintHandler);
    sem_init(&mutex_allot_server_id, 0, 1);
    sem_init(&mutex_write_batch, 0, 1);
    sem_init(&split_db_sem, 0, 1);
    sem_init(&split_db_info_mutex, 0, 1);
    init_zk_connection();

    // Check if master is active or not. If file exists, then there's master, else, no master, so create file and become master
    populate_cur_node_server_details();
    
    find_master_server(); 
    get_config();
    
    if(!isMaster) {
        // get current server details and the server map from the master
        p2p::ServerInit idreply = get_cur_server_details_and_initialize_map();

        // Create server path if it doesn't exist, should be done before calling split/merge db
        // and after getting server id from master.
        init_server_dir();
        
        //start p2p server
        init_p2p_server();

        // add sync grpc call letting the master know it's there
        ping_master_wrapper(idreply);

        //update_ring_id(); //TODO: this function will be deprecated
        successor_server_details = find_successor(server_details);

        //Contact successor and transfer keys belonging to current node
        split_db_wrapper();
        
        if (do_cache) init_cache_dir(); //Delete old cache and create new cache directory 
        
        print_ring();

        //initialize new server with the master and keep checking if master is alive
        init_server_and_watch_master(idreply);

    } else {
        init_p2p_server();

        // Create server path if it doesn't exist
        init_server_dir();

        if (do_cache) init_cache_dir(); //Delete old cache and create new cache directory 

        insert_server_entry(server_details);
    }

    cur_node_wifs_address = getWifsServerAddr(server_details);
    // spin off level db server locally
    leveldb::Options options;
    options.create_if_missing = true;

    // now that we are doing consistent hashing and levelDB is being spun up on EFS, no need to use 
    // the custom storage interface, right?
    // leveldb::Env* actual_env = leveldb::Env::Default();
    // leveldb::Env* env = new CustomEnv(actual_env);
    // options.env = env;

    // std::cout << getServerDir(server_details.serverid()) <<std::endl;
    leveldb::Status status = leveldb::DB::Open(options, getServerDir(server_details.serverid()).c_str(), &db);
    if(do_cache){
        leveldb::Status s_cache = leveldb::DB::Open(options, getCacheDir(server_details.serverid()).c_str(), &cache);
        if(!s_cache.ok()){
            do_cache = 0;
            std::cout << "Unable to create Cache DB. Caching disabled." <<std::endl;
        }
    }
    
    assert(status.ok());
   
    if(isMaster) collect_db();      //collect all existing leveldb dbs and put in server0's db

    run_wifs_server();   
    return 0;
}

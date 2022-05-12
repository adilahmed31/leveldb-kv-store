#include <grpc++/grpc++.h>
#include <grpc/impl/codegen/status.h>
#include <grpcpp/impl/codegen/status_code_enum.h>
#include <csignal>

#include <chrono>
#include <thread>

#include "commonheaders.h"
#include "wifs.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Status;
using grpc::StatusCode;

using wifs::GetReq;
using wifs::GetRes;
using wifs::WIFS;
using wifs::PutReq;
using wifs::PutRes;
using wifs::DeleteReq;
using wifs::DeleteRes;
using wifs::ServerDetails;

#define BLOCK_SIZE 100000

class WifsClient {
   public:
    WifsClient(std::shared_ptr<Channel> channel) : stub_(WIFS::NewStub(channel)) {}

    void print_map(const google::protobuf::Map<long, wifs::ServerDetails> &ht) {
        for(auto it = ht.begin() ; it != ht.end() ; it++) {
            std::cout<<it->first<<" - "<<it->second.serverid()<<"\n";
        }
    }

    int wifs_GETRANGE(char* key, std::vector<wifs::KVPair> &batch_read){
        //range read client side code
        ClientContext context;
        GetReq request;
        GetRes reply;
        request.set_key(std::string(key));
        request.set_mode(1);
        Status status = stub_->wifs_GET(&context, request, &reply);
        std::vector<wifs::KVPair> batch_results(reply.kvpairs().begin(), reply.kvpairs().end());
        batch_read = batch_results;
        // print_map(reply.hash_server_map());
        return status.ok() ? 0 : -1;
    }

    int wifs_GET(char* key, char* val) {

        ClientContext context;
        GetReq request;
        GetRes reply;
        request.set_key(std::string(key));
        request.set_mode(0);
        Status status = stub_->wifs_GET(&context, request, &reply);
        // print_map(reply.hash_server_map());
        server_map = std::map<long,wifs::ServerDetails>(reply.hash_server_map().begin(), reply.hash_server_map().end());
        int buffer_length = strlen(reply.val().c_str());
        strcpy(val, reply.val().c_str()) ;
        return status.ok() && reply.status() == wifs::GetRes_Status_PASS ? 0 : -1;
    }

    int wifs_PUT(char* key, const char* val) {
        ClientContext context;
        PutReq request;
        PutRes reply;
        request.set_key(std::string(key));
        request.set_val(std::string(val));

        Status status = stub_->wifs_PUT(&context, request, &reply);
        // print_map(reply.hash_server_map());
        server_map = std::map<long,wifs::ServerDetails>(reply.hash_server_map().begin(), reply.hash_server_map().end());
        return status.ok() && reply.status() == wifs::PutRes_Status_PASS ? 0 : -1;
    }

    int wifs_DELETE(char* key){
        ClientContext context;
        DeleteReq request;
        DeleteRes reply;
        request.set_key(std::string(key));
        
        Status status = stub_->wifs_DELETE(&context, request, & reply);
        // print_map(reply.hash_server_map());
        server_map = std::map<long,wifs::ServerDetails>(reply.hash_server_map().begin(), reply.hash_server_map().end());
        return status.ok() ? 0 : -1;
    }
    

   private:
    std::unique_ptr<WIFS::Stub> stub_;
};

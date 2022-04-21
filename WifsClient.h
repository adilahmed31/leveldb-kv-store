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

#define BLOCK_SIZE 4096

class WifsClient {
   public:
    WifsClient(std::shared_ptr<Channel> channel) : stub_(WIFS::NewStub(channel)) {}
    int wifs_GET(int key, char val[BLOCK_SIZE]) {
        ClientContext context;
        GetReq request;
        GetRes reply;
        request.set_key(key);
        Status status = stub_->wifs_GET(&context, request, &reply);
        strncpy(val, reply.val().c_str(), BLOCK_SIZE);
        return status.ok() ? 0 : -1;
    }

    int wifs_PUT(int key, char val[BLOCK_SIZE]) {
        ClientContext context;
        PutReq request;
        PutRes reply;
        request.set_key(key);
        request.set_val(std::string(val));

        Status status = stub_->wifs_PUT(&context, request, &reply);
        return status.ok() ? 0 : -1;
    }

   private:
    std::unique_ptr<WIFS::Stub> stub_;
};

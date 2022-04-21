#include <iostream>
#include "leveldb/db.h"
#include "leveldb/env.h"

using namespace leveldb;

class CustomEnv : public leveldb::Env {
    public:
    CustomEnv(Env* t) : target_(t) {}
    ~CustomEnv();

    // Return the target to which this Env forwards all calls.
    Env* target() const { return target_; }

    // The following text is boilerplate that forwards all methods to target().
    Status NewSequentialFile(const std::string& f, SequentialFile** r) override {
        return target_->NewSequentialFile(f, r);
    }
    Status NewRandomAccessFile(const std::string& f, RandomAccessFile** r) override {
        return target_->NewRandomAccessFile(f, r);
    }
    
    Status NewWritableFile(const std::string& f, WritableFile** r) override {
        return target_->NewWritableFile(f, r);
    }
    
    Status NewAppendableFile(const std::string& f, WritableFile** r) override {
        return target_->NewAppendableFile(f, r);
    }
    
    bool FileExists(const std::string& f) override {
        return target_->FileExists(f);
    }
    
    Status GetChildren(const std::string& dir, std::vector<std::string>* r) override {
        return target_->GetChildren(dir, r);
    }
    
    Status RemoveFile(const std::string& f) override {
        return target_->RemoveFile(f);
    }
    
    Status CreateDir(const std::string& d) override {
        return target_->CreateDir(d);
    }
    
    Status RemoveDir(const std::string& d) override {
        return target_->RemoveDir(d);
    }
    
    Status GetFileSize(const std::string& f, uint64_t* s) override {
        return target_->GetFileSize(f, s);
    }
    
    Status RenameFile(const std::string& s, const std::string& t) override {
        return target_->RenameFile(s, t);
    }
    
    Status LockFile(const std::string& f, FileLock** l) override {
        return target_->LockFile(f, l);
    }
    
    Status UnlockFile(FileLock* l) override { 
        return target_->UnlockFile(l); 
    }
    
    void Schedule(void (*f)(void*), void* a) override {
        return target_->Schedule(f, a);
    }
    
    void StartThread(void (*f)(void*), void* a) override {
        return target_->StartThread(f, a);
    }
    
    Status GetTestDirectory(std::string* path) override {
        return target_->GetTestDirectory(path);
    }
    
    Status NewLogger(const std::string& fname, Logger** result) override {
        return target_->NewLogger(fname, result);
    }
    
    uint64_t NowMicros() override { 
        return target_->NowMicros(); 
    }
    
    void SleepForMicroseconds(int micros) override {
        target_->SleepForMicroseconds(micros);
    }

    private:
    Env* target_;
};

CustomEnv::~CustomEnv() {

}

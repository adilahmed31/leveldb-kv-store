#include <bits/stdc++.h>

#include <conservator/ConservatorFrameworkFactory.h>
#include <conservator/ConservatorFramework.h>
#include <conservator/ExistsBuilder.h>
#include <check.h>

using namespace std;

string zk_server_addr = "127.0.0.1:2181";

int main(int argc, char** argv) {
    if(argc < 3) {
        cout<<"Expected usage: set_zk_config <zk server address> <config string>";
        return 0;
    }

    zk_server_addr = std::string(argv[1]);
    
    ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
    unique_ptr<ConservatorFramework> framework = factory.newClient(zk_server_addr.c_str());
    framework->start();

    framework->deleteNode()->deletingChildren()->forPath("/config");
    framework->create()->forPath("/config", argv[2]);
    framework->close();

    return 0;
}
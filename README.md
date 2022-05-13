# DISCO
DISCO is a key-value store based on levelDB.

## Pre-requisites

- Install levelDB (https://github.com/google/leveldb)
- Install gRPC (https://grpc.io/docs/languages/cpp/quickstart/)
- Install Zookeeper (instructions below)

## Compilation

Run the below commands to compile the executables on both servers

  ```
  cd code/
  mkdir build/  
  cd build/  
  cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../..  
  make 
```
## Execution

Run the server using `sudo`. Provide the path to the zookeeper node and the path to shared storage mount path. If no values are provided, the default are localhost and the current node's home directory.

E.g:
`sudo ./server c220g1-030604.wisc.cloudlab.us:2181 /ubunut/mnt/efs`
`sudo ./server`

The configuration consists of three comma-seperated values: mode,buffer\_size, prefix\_length. 

Update the configuration using the set\_zk\_config executable.

E.g. ./set\_zk\_config c220g1-030604.wisc.cloudlab.us:2181 1,1,1

## Tests
While the testing framework defined in `tester.py` allows  the server to be started-up dynamically, the current tests rely on the server being already present. The zookeeper node address is hardcoded in each test and needs to be updated for the test to run successfully.

### Running the ZK server

mvn
```
  wget https://dlcdn.apache.org/maven/maven-3/3.8.5/binaries/apache-maven-3.8.5-bin.zip
  unzip apache-maven-3.8.5-bin.zip
  export PATH=~/apache-maven-3.8.5/bin:$PATH

  sudo apt-get update
  sudo apt install default-jre ivy lcov check libcppunit-dev

  wget https://dlcdn.apache.org/zookeeper/zookeeper-3.8.0/apache-zookeeper-3.8.0.tar.gz
  tar xvzf apache-zookeeper-3.8.0.tar.gz
  cd apache-zookeeper-3.8.0/zookeeper-jute/
  mvn compile
  cd ../zookeeper-client/zookeeper-client-c/
  autoreconf -if
  ./configure
  make
  sudo make install

  cd ~
  git clone https://github.com/rjenkins/conservator.git
  cd conservator/
```
change 
``` 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")' ``` in CMakeLists.txt to 
```set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -DTHREADED -DHAVE_OPENSSL_H")'```

```
  mkdir build
  cd build/
  cmake ..
  make
  sudo make install
```
  for ZK server
  https://phoenixnap.com/kb/install-apache-zookeeper
```
  cd ~
  sudo useradd zookeeper -m
  sudo usermod --shell /bin/bash zookeeper
  sudo passwd zookeeper
  sudo usermod -aG sudo zookeeper
  sudo getent group sudo
  sudo mkdir -p /data/zookeeper
  sudo chown -R zookeeper:zookeeper /data/zookeeper
  cd /opt
  sudo wget https://dlcdn.apache.org/zookeeper/zookeeper-3.8.0/apache-zookeeper-3.8.0-bin.tar.gz
  sudo tar -xvf apache-zookeeper-3.8.0-bin.tar.gz 
  sudo mv apache-zookeeper-3.8.0-bin zookeeper
  sudo chown -R zookeeper:zookeeper /opt/zookeeper
```
#### paste contents from site
```
  sudo vi /opt/zookeeper/conf/zoo.cfg
  cd zookeeper
```
#### start ZK server
```
  sudo bin/zkServer.sh start
```
#### stop ZK server 
```
  sudo bin/zkServer.sh stop
```

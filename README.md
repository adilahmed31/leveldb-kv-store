# leveldb-kv-store
## Compilation

Run the below commands to compile the executables on both servers

  ```
  cd leveldb-kv-store
  mkdir -p cmake/build  
  cd cmake/build  
  cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../..  
  make 
```

for ZK client

# mvn
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
# change 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")' in CMakeLists.txt to 
# 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -DTHREADED -DHAVE_OPENSSL_H")'
mkdir build
cd build/
cmake ..
make
sudo make install
for ZK server
https://phoenixnap.com/kb/install-apache-zookeeper

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

# paste contents from site
sudo vi /opt/zookeeper/conf/zoo.cfg

cd zookeeper
# start ZK server
sudo bin/zkServer.sh start

# stop ZK server 
sudo bin/zkServer.sh stop

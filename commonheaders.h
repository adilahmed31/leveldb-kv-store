#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#define MAX_PATH_LENGTH 1000
#define HEARTBEAT_TIMER 1000
#define RINGLENGTH 255
#define MAX_NUM_SERVERS 100 //limiting max servers to use static array for client_stub - can be depreciated if client_stub need not be reused

int read_index = 0;
int primary_index = 0;
int single_server = 0;

std::string getServerDir(int machine_id){
        return "/users/oahmed4/.server" +  std::to_string(machine_id);
}

std::string getServerPath(int machine_id) {
    return getServerDir(machine_id) + "/kv" ;//"/file_" + address;
}

std::string getLastAddressPath(int machine_id) {
    return getServerDir(machine_id) + "/lastaddr" ;//"/file_" + address;
}

std::string getP2PServerAddr(int machine_id){
        return "localhost:" +  std::to_string(50060+machine_id);
}

std::string getWifsServerAddr(int machine_id){
        return "localhost:" +  std::to_string(50070+machine_id);
}

//Consistent hashing - modify/replace hash function if required
unsigned int somehashfunction(unsigned int x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    // printf("%d: x before\n", x);
    x = x % RINGLENGTH;
    // printf("%d: x after\n", x);
    return x ;
}

//Save list of servers in ascending order of ranges of keys handled

typedef struct server_list_element{
  int server_id;
  long max_key_hash;
  struct server_list_element *next;
}server_list_element;

server_list_element* server_list_root;

server_list_element *create_server_entry(int server_id, long max_key_hash){
  server_list_element *new_el = (server_list_element *) malloc(sizeof(server_list_element));
  new_el->server_id = server_id;
  new_el->max_key_hash = max_key_hash;
  new_el->next = NULL; 
  return new_el;
}

void insert_server_entry(int server_id){
  server_list_element *cur_el = server_list_root;
  server_list_element *prev_el = NULL;
  server_list_element *new_el = create_server_entry(server_id,somehashfunction(server_id));

  // std::cout << "new serverid: " << new_el->server_id << ", max_key_hash: " << new_el->max_key_hash << std::endl;S
  while(cur_el!=NULL){
    if(new_el->max_key_hash < cur_el->max_key_hash){
      //check if cur_el is last element(with max_key 255)
    // std::cout << "cur serverid: " << cur_el->server_id << ", max_key_hash: " << cur_el->max_key_hash << std::endl;
      if(cur_el->max_key_hash == RINGLENGTH){
        long cur_el_max_key = somehashfunction(cur_el->server_id);
        if(cur_el_max_key < new_el->max_key_hash){
          //set new_el as last element
          cur_el->max_key_hash = cur_el_max_key;
          cur_el->next = new_el;
          new_el->max_key_hash = RINGLENGTH;
          return;
        }
      }
      //insert new_el before cur_el
      new_el->next = cur_el;
      if(prev_el!=NULL) prev_el->next = new_el;
      else server_list_root = new_el;
      return;
    }
    prev_el = cur_el;
    cur_el = cur_el->next;
  }
}

void print_ring(){
  std::cout << "Printing Ring ---- " << std::endl;
  server_list_element *cur_el = server_list_root;
  while(cur_el!=NULL){
    std::cout << "serverid: " << cur_el->server_id << ", max_key_hash: " << cur_el->max_key_hash << std::endl;
    cur_el = cur_el->next;
  }
}


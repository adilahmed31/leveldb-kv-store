from ctypes import *
import os
from tester import *
import time

import random
import string
from itertools import compress

import threading
import sys

'''
Test workloads:
---------------
Keys are 16 bytes each.
Value are 100 bytes each (with enough redundancy so that a simple compressor shrinks them to 50% of their original size).
Sequential reads/writes traverse the key space in increasing order.
Random reads/writes traverse the key space in random order.
'''

class Workloads(object):

    def __init__(self, client, num_keys, random = True):
        self.key_size = 16
        self.val_size = 100
        self.client = client
        self.num_keys = num_keys
        self.kv = self.generate_kv_pairs(num_keys, random)
        
    #Generate random string of size str_length
    def generate_rand_string(self, str_length):
        return ''.join(random.choices(string.ascii_uppercase + string.ascii_lowercase, k=str_length))

    # TODO: figure out how to get sequential keys
    def generate_seq_string(self):
        pass

    #Generate 'num_keys' random keys and values
    def generate_kv_pairs(self, num_keys, random):
        kv = {}
        for i in range(num_keys):
            if random:
                kv[self.generate_rand_string(self.key_size)] = self.generate_rand_string(self.val_size)
            else:
                kv[self.generate_seq_string(self.key_size)] = self.generate_rand_string(self.val_size)

        # print(f"Generated {num_keys} key value pairs")
        return kv

    #One client, puts all keys and values in kv
    def test_write_perc(self, percentage_writes):
        reads = random.choices([True, False], weights = [100 - percentage_writes, percentage_writes], k=self.num_keys)
        num_reads = sum(reads)
        num_writes = self.num_keys - num_reads
        # print("Number of reads: ", num_reads)
        # print("Number of writes: ", num_writes)
        # print(f"Write Percentage: Expected - {1.0*percentage_writes}%, Actual - {100*num_writes/num_keys}%")

        #Put all keys we want to test get on
        for key in list(compress(self.kv.keys(), reads)):
            self.client.put(key, self.kv[key])

        #Test now
        start_time = time.time()
        
        for i, key in enumerate(self.kv):
            if reads[i]:
                self.client.get(key)
            self.client.put(key, self.kv[key])
        
        time_wl = time.time() - start_time
        print(f"Time to complete: {time_wl}")


if __name__ == "__main__":
    num_clients = int(sys.argv[1])
    write_percent = int(sys.argv[2])
    client = [0]*(num_clients+1)
    test_rand_workloads = []
    num_keys = 100
    for i in range(num_clients):
        client[i] = Client("c220g1-030604.wisc.cloudlab.us:2181")
        test_rand_workloads.append(Workloads(client[i], num_keys))    
    for i in range(num_clients):
        threading.Thread(target=test_rand_workloads[i].test_write_perc(write_percent))
   

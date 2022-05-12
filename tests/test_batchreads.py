from ctypes import *
import os
from tester import *
import time

import random
import string

'''
Test workloads:
---------------
Keys are 16 bytes each.
Value are 100 bytes each (with enough redundancy so that a simple compressor shrinks them to 50% of their original size).
Sequential reads/writes traverse the key space in increasing order.
Random reads/writes traverse the key space in random order.
'''

class Workloads(object):

    def __init__(self, client, num_keys, prefix, random = True): 
        self.key_size = 16
        self.val_size = 2
        self.client = client
        self.prefix = prefix
        self.kv = self.generate_kv_pairs(num_keys, random)
    
    #Generate strings with same prefix for keys
    def generate_string_with_prefix(self, str_length):
        return self.prefix + ''.join(random.choices(string.ascii_uppercase + string.ascii_lowercase, k=str_length-len(self.prefix)))

    #Generate random string of size str_length for values
    def generate_rand_string(self, str_length):
        return ''.join(random.choices(string.ascii_uppercase + string.ascii_lowercase, k=str_length))

    #Generate 'num_keys' random keys and values
    def generate_kv_pairs(self, num_keys, random):
        kv = {}
        for i in range(num_keys):
            kv[self.generate_string_with_prefix(self.key_size)] = self.generate_rand_string(self.val_size)

        print(f"Generated {num_keys} key value pairs")
        return kv

    #One client, puts all keys and values in kv
    def send_puts(self):
        start_time = time.time()
        
        for key in self.kv:
            print(key)
            self.client.put(key, self.kv[key])
        
        time_put = time.time() - start_time
        print(f"Time for prefix puts (single client): {time_put}")

    #One client, gets values for all prefix keys
    def send_gets(self):
        start_time = time.time()

        batchread_size = self.client.get_range_nilext(self.prefix+"*")
        
        time_get = time.time() - start_time
        print(f"Time for batch gets (single client): {time_get}, batchread size: {batchread_size}")

if __name__ == "__main__":
    client = Client()
    num_keys = 20
    prefix = "kalyani"

    test_rand_workload = Workloads(client, num_keys, prefix)
    test_rand_workload.send_puts()
    test_rand_workload.send_gets()

    
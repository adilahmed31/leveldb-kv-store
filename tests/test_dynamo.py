import boto3

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

    def __init__(self, num_keys, random = True):
        # Get the service resource.
        self.dynamodb = boto3.resource('dynamodb')
        self.table = dynamodb.Table('benchmarking')
        self.key_size = 16
        self.val_size = 100
        self.kv = self.generate_kv_pairs(num_keys, random)
        
    #Generate random string of size str_length
    def generate_rand_string(str_length):
        return ''.join(random.choices(string.ascii_uppercase + string.ascii_lowercase, k=str_length))

    # TODO: figure out how to get sequential keys
    def generate_seq_string():
        pass

    #Generate 'num_keys' random keys and values
    def generate_kv_pairs(num_keys, random):
        kv = {}
        for i in range(num_keys):
            if random:
                kv[generate_rand_string(KEY_SIZE)] = generate_rand_string(VAL_SIZE)
            else:
                kv[generate_seq_string(KEY_SIZE)] = generate_rand_string(VAL_SIZE)

        print(f"Generated {num_keys} key value pairs")
        return kv

    #One client, puts all keys and values in kv
    def send_puts():
        time_put = 0
        start_time = time.time()
        
        with table.batch_writer() as batch:
            for key in self.kv:
                batch.put_item(
                Item={
                    'key': key,
                    'value': kv[key]
                }
            )

        time_put = time.time() - start_time
        print(f"Time for random puts (single client): {time_put}")

    #One client, gets values for all keys in kv
    def send_gets():
        time_get = 0
        start_time = time.time()
        
        for key in self.kv:
            self.client.get(key)
            response = table.get_item(
                Key={
                    'key': key
                }
            )
            #To access value, do:
            # item = response['Item']
            # print(item)

        time_get = time.time() - start_time
        print(f"Time for random gets (single client): {time_get}")

if __name__ == "__main__":
    client = Client()
    num_keys = 100

    test_rand_workload = Workloads(client, num_keys)
    test_rand_workload.send_puts()
    test_rand_workload.send_gets()

    test_seq_workload = Workloads(client, num_keys, random=False)
    test_seq_workload.send_puts()
    test_seq_workload.send_gets()
    

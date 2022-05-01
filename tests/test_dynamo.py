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

class DynamoWorkloads(object):

    def __init__(self, num_keys, random = True):
        # Get the service resource.
        self.dynamodb = boto3.resource('dynamodb')
        self.table = self.dynamodb.Table('benchmarking')
        self.key_size = 16
        self.val_size = 100
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

        print(f"Generated {num_keys} key value pairs")
        return kv

    #One client, puts all keys and values in kv
    def send_puts(self):
        time_put = 0
        start_time = time.time()
        
        for key in self.kv:
            self.table.put_item(
                Item={
                    'key': key,
                    'value': self.kv[key]
                }
            )

    ''' FOR BATCH MODE, USE - 
        with self.table.batch_writer() as batch:
            for key in self.kv:
                batch.put_item(
                Item={
                    'key': key,
                    'value': self.kv[key]
                }
                )
        '''

        time_put = time.time() - start_time
        print(f"Time for random puts (single client): {time_put}")

    #One client, gets values for all keys in kv
    def send_gets(self):
        time_get = 0
        start_time = time.time()
        
        for key in self.kv:
            response = self.table.get_item(
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
    num_keys = 100

    test_rand_workload = DynamoWorkloads(num_keys)
    test_rand_workload.send_puts()
    test_rand_workload.send_gets()

#    test_seq_workload = DynamoWorkloads(num_keys, random=False)
#    test_seq_workload.send_puts()
#    test_seq_workload.send_gets()
    

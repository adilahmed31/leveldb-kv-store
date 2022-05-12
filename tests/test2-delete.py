from ctypes import *
import os
from tester import *
import time

# Test 1: Write a key-value pair and read the bytes

#Client Steps
#Step 1: Init the client
client = Client("http://c220g1-030604.wisc.cloudlab.us:2181/")

#Step 2: Perform the put operation
client1.put("delete_test", "testing123")

#Step 3: Perform the read
read_val = client1.get("delete_test")

#Step 4: Check result
print(read_val)

client1.delete("delete_test")

read_val = client1.get("delete_test")

print(read_val)
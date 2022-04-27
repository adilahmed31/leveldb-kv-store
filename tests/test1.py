from ctypes import *
import os
from tester import *
import time

# Test 1: Write a key-value pair and read the bytes

#The helper functions defined in tester.py convert the python objects to c objects that can be used for the functions. 
#Use the same functions for generating ip addresses, write buffers and read buffers

#Client Steps
#Step 1: Init the client with the server ID
client1 = Client()

#Step 2: Perform the put operation
client1.put("l1fsdf", "f" * 4096)

#Step 3: Perform the read
# client1.get("k")

#Step 4: Check result
# print(client1.read_buf)
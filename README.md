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

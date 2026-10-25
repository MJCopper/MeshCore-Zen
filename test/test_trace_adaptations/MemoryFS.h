#pragma once
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <cstring>
class File {
  std::shared_ptr<std::vector<uint8_t>> _data;
  size_t _at=0;
public:
  File()=default;
  explicit File(std::shared_ptr<std::vector<uint8_t>> data):_data(data){}
  explicit operator bool() const{return bool(_data);}
  size_t size()const{return _data?_data->size():0;}
  size_t read(uint8_t* out,size_t count){if(!_data)return 0;count=std::min(count,size()-_at);memcpy(out,_data->data()+_at,count);_at+=count;return count;}
  size_t write(const uint8_t* data,size_t n){if(!_data)return 0;_data->insert(_data->end(),data,data+n);return n;}
  void close(){_data.reset();}
};
class MemoryFS {
public:
  std::map<std::string,std::shared_ptr<std::vector<uint8_t>>> files;
  bool fail_write=false,fail_rename=false;
  std::string fail_path;
  File open(const char* path,const char* mode="r"){
    if(mode[0]=='w'){
      if(fail_write || fail_path==path)return File();
      files[path]=std::make_shared<std::vector<uint8_t>>();
    }
    auto it=files.find(path);return it==files.end()?File():File(it->second);
  }
  bool exists(const char* path)const{return files.count(path);}
  bool remove(const char* path){return files.erase(path);}
  bool rename(const char* from,const char* to){
    if(fail_rename || !exists(from))return false;
    files[to]=files[from];files.erase(from);return true;
  }
};
#define FILESYSTEM MemoryFS

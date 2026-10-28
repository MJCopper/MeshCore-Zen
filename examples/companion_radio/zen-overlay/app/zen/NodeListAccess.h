#pragma once
#include "NodeListModel.h"

namespace zen {
// Templates keep the actual storage adapter host-testable without constructing
// the radio/BLE firmware. All reads are bounded values, with terminated names.
struct NodeListAccess {
  template<class Mesh,class Contact>
  static bool contact(Mesh& mesh,unsigned index,unsigned anonymous,NodeListModel::Record& out) {
    out=NodeListModel::Record{};
    if(index>=unsigned(mesh.getNumContacts()))return false;
    Contact value;
    const unsigned slot=index+anonymous;
    if(!mesh.getContactByIdx(slot,value))return false;
    memcpy(out.name,value.name,31);memcpy(out.pub_key,value.id.pub_key,32);
    out.type=value.type;out.has_key=true;out.favourite=value.isFav();
    out.lat_e6=value.gps_lat;out.lon_e6=value.gps_lon;
    out.lastmod=value.lastmod;out.contact_idx=slot;out.is_known=true;
    return true;
  }
  template<class Mesh,class Result>
  static bool discovery(Mesh& mesh,const Result* results,unsigned count,unsigned index,NodeListModel::Record& out) {
    out=NodeListModel::Record{};
    if(index>=count)return false;
    const Result& value=results[index];
    memcpy(out.name,value.name,31);memcpy(out.pub_key,value.pub_key,32);
    out.type=value.type;out.has_key=true;out.rssi=value.rssi;
    out.snr_x4=value.snr_x4;out.remote_snr_x4=value.remote_snr_x4;
    out.is_known=mesh.lookupContactByPubKey(value.pub_key,32)!=nullptr;
    return true;
  }
  template<class Advert>
  static bool advert(const Advert* cache,unsigned count,unsigned index,NodeListModel::Record& out) {
    out=NodeListModel::Record{};
    if(index>=count)return false;
    // Rank without reordering the shared cache. Equal times use slot order.
    const Advert* found=nullptr;
    for(unsigned i=0;i<count;++i) {
      unsigned rank=0;
      for(unsigned j=0;j<count;++j)
        if(cache[j].recv_timestamp>cache[i].recv_timestamp ||
           (cache[j].recv_timestamp==cache[i].recv_timestamp && j<i))++rank;
      if(rank==index) { found=&cache[i];break; }
    }
    if(!found)return false;
    memcpy(out.name,found->name,31);memcpy(out.pub_key,found->pubkey_prefix,7);
    out.type=1;out.lastmod=found->recv_timestamp;
    return true;
  }
};
} // namespace zen

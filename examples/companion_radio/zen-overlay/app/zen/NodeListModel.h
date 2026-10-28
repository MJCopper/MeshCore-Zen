#pragma once
#include <stdint.h>
#include <string.h>

namespace zen {
// A projection over MeshCore-owned records. Providers return one value at a
// time; no pointers survive a call and no arrays are copied into this model.
class NodeListModel {
public:
  enum Kind : uint8_t { CONTACT, ADVERT, DISCOVERY };
  enum Filter : uint8_t { ALL, FAV, RPT, ROOM, SNSR, FILTER_COUNT };
  static const char* const* filterLabels() {
    static const char* const labels[FILTER_COUNT]={"All","Fav","Rpt","Room","Snsr"};
    return labels;
  }
  struct Record {
    char name[32]{};
    uint8_t type=0, pub_key[32]{}; // adverts carry only their seven-byte prefix
    bool has_key=false, favourite=false;
    int32_t lat_e6=0, lon_e6=0;
    float dist_km=-1;
    uint32_t lastmod=0;
    int contact_idx=-1;
    int8_t rssi=0, snr_x4=0, remote_snr_x4=0;
    bool is_known=false;
  };
  // A usable infrastructure identity is required in both stored and scan views.
  static bool eligible(const Record& record) {
    return record.has_key && record.type>=2 && record.type<=4;
  }
  static constexpr unsigned CAPACITY=32;
private:
  struct Row {
    uint32_t identity[2]; // stale-reference detection only, never action identity
    union { float distance; uint32_t time; int32_t signal; } order;
    uint16_t index;
    uint8_t kind, favourite;
  } _rows[CAPACITY]{};
  static_assert(sizeof(Row)==16,"Node references must remain compact");
  Record _selected{}; // sole full identity snapshot, including across confirmations
  unsigned _count=0;
  uint32_t _generation=0;
  Kind _selected_kind=CONTACT;
  bool _valid=false;

  static void signature(const Record& record,uint32_t out[2]) {
    out[0]=2166136261u; out[1]=5381u;
    const uint8_t* key=record.pub_key;
    for(unsigned i=0;i<(record.has_key?32u:7u);++i) {
      out[0]=(out[0]^key[i])*16777619u; out[1]=out[1]*33u+key[i];
    }
    if(!record.has_key) for(unsigned i=0;i<sizeof(record.name);++i) {
      out[0]=(out[0]^uint8_t(record.name[i]))*16777619u;
      out[1]=out[1]*33u+uint8_t(record.name[i]);
    }
  }
  static bool same(const Record& a,const Record& b) {
    if(a.has_key!=b.has_key)return false;
    return a.has_key ? memcmp(a.pub_key,b.pub_key,32)==0 :
      memcmp(a.pub_key,b.pub_key,7)==0 && strncmp(a.name,b.name,32)==0;
  }
  static bool accepts(const Record& r,Filter filter) {
    if(!eligible(r))return false;
    switch(filter) {
      case ALL:return true;
      case FAV:return r.favourite;
      case RPT:return r.type==2;
      case ROOM:return r.type==3;
      case SNSR:return r.type==4;
      default:return false;
    }
  }
public:
  unsigned count() const { return _count; }
  const Record* selected() const { return _valid?&_selected:nullptr; }
  void clearSelection() { _valid=false; }
  template<class Provider> bool read(Provider& p,unsigned row,Record& out) const {
    if(row>=_count)return false;
    const Row& ref=_rows[row]; uint32_t tag[2];
    if(!p.read(Kind(ref.kind),ref.index,out) || !eligible(out))return false;
    signature(out,tag);
    return tag[0]==ref.identity[0] && tag[1]==ref.identity[1];
  }
  template<class Provider> bool select(Provider& p,unsigned row) {
    Record value{};
    _valid=read(p,row,value);
    if(_valid) {
      _selected=value;_selected_kind=Kind(_rows[row].kind);
      _generation=p.generation();
    }
    return _valid;
  }
  // Actions always resolve the complete saved identity, never the row hash or
  // a possibly recycled contact index. New discovery sessions invalidate scans.
  template<class Provider> bool validate(Provider& p) {
    if(!_valid)return false;
    if(_selected_kind==DISCOVERY && _generation!=p.generation()) {
      _valid=false;return false;
    }
    Record value{};
    for(unsigned i=0;i<p.count(_selected_kind);++i)
      if(p.read(_selected_kind,i,value) && eligible(value) && same(value,_selected)) {
        _selected=value;return true;
      }
    _valid=false;return false;
  }
  template<class Provider> int rebuild(Provider& p,bool scan,Filter filter,bool by_time,uint32_t now) {
    // Keep the original snapshot while building; it may still be an action target.
    bool keep=validate(p);
    _count=0;
    const Kind kind=scan?DISCOVERY:CONTACT;
    Record value{};
    // Untyped adverts remain in MeshCore's shared cache, but cannot establish
    // infrastructure membership. Never merge them into this management view.
    const unsigned limit=p.count(kind);
    for(unsigned index=0;index<limit && _count<CAPACITY;++index) {
      if(!p.read(kind,index,value) || !accepts(value,filter))continue;
      Row& row=_rows[_count++];
      signature(value,row.identity);row.index=index;row.kind=kind;
      row.favourite=value.favourite;
      if(scan)row.order.signal=value.rssi;
      else if(by_time)row.order.time=value.lastmod && value.lastmod<=now?value.lastmod:0;
      else row.order.distance=value.dist_km;
    }
    // Selection sort preserves the existing tie behaviour and admission policy.
    for(unsigned i=0;i+1<_count;++i) {
      unsigned best=i;
      for(unsigned j=i+1;j<_count;++j) {
        const Row &a=_rows[j], &b=_rows[best];
        bool before;
        if(scan)before=a.order.signal>b.order.signal;
        else if(a.favourite!=b.favourite)before=a.favourite;
        else if(by_time)before=a.order.time>b.order.time;
        else before=a.order.distance>=0 && (b.order.distance<0 || a.order.distance<b.order.distance);
        if(before)best=j;
      }
      if(best!=i) { Row temp=_rows[i];_rows[i]=_rows[best];_rows[best]=temp; }
    }
    if(keep)for(unsigned i=0;i<_count;++i)
      if(read(p,i,value) && same(value,_selected))return int(i);
    return -1;
  }
};
static_assert(sizeof(NodeListModel)<=640,"Node List model exceeded its RAM budget");
} // namespace zen

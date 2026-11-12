#include "layer3/bplus.hpp"
#include <iostream>
#include <filesystem>
#include <random>
#include <vector>
using namespace cbd;
class Input final: public RecordStream {
 const std::vector<unsigned>& sizes_; unsigned next_=0; std::vector<std::byte> rec_;
 public: Input(const std::vector<unsigned>& s):sizes_(s){}
 std::optional<std::span<const std::byte>> next() override {
  if(next_==sizes_.size())return std::nullopt;
  rec_.assign(sizes_[next_]-4,std::byte(next_%251)); store<std::int32_t>(rec_.data(),next_++); return rec_;
 }
};
bool feasible(const std::vector<unsigned>& sizes){
 std::vector<bool> ok(sizes.size()+1); ok[0]=true;
 unsigned total=0;for(auto n:sizes)total+=n; if(total<=4072)return true;
 for(unsigned i=0;i<sizes.size();i++)if(ok[i]){unsigned sum=0; for(unsigned j=i;j<sizes.size();j++){sum+=sizes[j];if(sum>4072)break;if(sum>=2036)ok[j+1]=true;}}
 return ok.back();
}
int main(){
 std::mt19937 rng(452101); unsigned good=0, impossible=0;
 char pattern[]="/tmp/cbd001-review-fuzz-XXXXXX"; std::string dir=mkdtemp(pattern);
 for(unsigned t=0;t<2500;t++){
  std::vector<unsigned> sizes;
  if(t==0)sizes={1400,1400,1000,1000,2100};
  else if(t==1)sizes={1000,1000,2100};
  else if(t==2)sizes={1500,1500,1000};
  else {unsigned n=1+rng()%140; for(unsigned i=0;i<n;i++)sizes.push_back(t%3==0 ? 8+rng()%126 : 8+rng()%3500);}
  double fill=std::vector<double>{.01,.1,.5,.7,1.0}[t%5];
  auto path=dir+"/"+std::to_string(t); Config c;c.data_dir=path;
  bool want=feasible(sizes), loaded=false;
  {
   BufferPool pool(c);auto f=pool.create("l",FileKind::Data), nf=pool.create("n",FileKind::Index);
   auto schema=make_schema({{"k",FieldType::Int32,0,true}});
   BPlusEngine tree(pool,f,nf,KeyCodec::for_field(schema.field(0)),[](auto r)->Key{return load<std::int32_t>(r.data());},true,PageType::Leaf,0);
   tree.init_empty(); Input input(sizes);
   try{tree.bulk_load(input,fill);loaded=true;}catch(const std::exception& e){if(want){std::cout<<"FAIL throw t="<<t<<" "<<e.what()<<'\n';return 1;}}
   if(loaded!=want){std::cout<<"FAIL feasibility t="<<t<<'\n';return 1;}
   if(loaded){
    auto err=tree.verify(true);if(!err.empty()){std::cout<<"FAIL verify t="<<t<<" "<<err<<'\n';return 1;}
    unsigned seen=0,prev=0,leaves=0;
    for(auto b=tree.meta().first_leaf;b;){auto page=pool.fetch({f,b},kSeqOnce,kNoReservation);auto h=read_header(page.read());
     if(++leaves>sizes.size()||h.prev!=prev){std::cout<<"FAIL links t="<<t<<'\n';return 1;}
     for(auto entry:sorted_slots(page.read(),tree.key_of())){auto r=sp_record(page.read(),entry.second); if(seen>=sizes.size()||r.size()+4!=sizes[seen]||load<std::int32_t>(r.data())!=int(seen)){std::cout<<"FAIL data t="<<t<<'\n';return 1;} for(unsigned j=4;j<r.size();j++)if(r[j]!=std::byte(seen%251)){std::cout<<"FAIL payload t="<<t<<'\n';return 1;} seen++;}
     prev=b;b=h.next;
    }
    if(seen!=sizes.size()){std::cout<<"FAIL count t="<<t<<'\n';return 1;}
    for(unsigned k=0;k<sizes.size();k++){auto rid=tree.find_first(Key{int(k)});if(!rid){std::cout<<"FAIL lookup t="<<t<<'\n';return 1;}auto p=pool.fetch({f,rid->block},kRandomReuse,kNoReservation);if(load<std::int32_t>(sp_record(p.read(),rid->slot).data())!=int(k)){std::cout<<"FAIL rid t="<<t<<'\n';return 1;}}
    ++good;
   }else ++impossible;
  }
  std::filesystem::remove_all(path);
 }
 std::filesystem::remove_all(dir);std::cout<<"PASS 2500 cases; valid="<<good<<" impossible="<<impossible<<'\n';
}

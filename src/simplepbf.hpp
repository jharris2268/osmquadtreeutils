#ifndef SIMPLEPBF_HPP
#define SIMPLEPBF_HPP

#include <string>
#include <vector>
#include <memory>
#include <boost/tuple/tuple.hpp>
#include <boost/python.hpp>

typedef long long int64;
typedef unsigned long long uint64;
typedef boost::tuple<uint64, uint64, std::string> PbfTag;


uint64 readUVarint(const std::string& data, size_t& pos);
int64 unZigZag(uint64 uv);
int64 readVarint(const std::string& data, size_t& pos);
std::string readData(const std::string& data, size_t& pos);

double toDouble(uint64 uv);

PbfTag readPbfTag(const std::string& data, size_t& pos);

std::vector<int64> readPackedDelta(const std::string& data);
std::vector<uint64> readPackedInt(const std::string& data);



struct bbox {
    int64 minx,miny,maxx,maxy;
    
    bbox() : minx(1800000000),miny(1800000000),maxx(-1800000000),maxy(-1800000000) {}
    bbox(int64 a, int64 b, int64 c, int64 d) : minx(a),miny(b),maxx(c),maxy(d) {}
};

class geometry {
    public:
        geometry(uint64 ty, bbox bb) : ty_(ty), bb_(bb) {}
        
        uint64 Type() { return ty_; }
        bbox Bbox() { return bb_; }
        
        virtual ~geometry() {}
        
    private:
        uint64 ty_;
        bbox bb_;
        
};

struct tag {
    tag() : key(""), val("") {}
    tag(std::string k, std::string v) : key(k), val(v) {}
    std::string key,val;
};
//typedef std::pair<std::string,std::string> tag;

struct info {
    info() : version(0),timestamp(0),changeset(0),user_id(0),user(""),visible(false) {}
    info(int64 vs, int64 ts, int64 cs, int64 ui, std::string us) : 
        version(vs),timestamp(ts),changeset(cs),user_id(ui),user(us),visible(true) {}
    
    int64 version;
    int64 timestamp;
    int64 changeset;
    int64 user_id;
    std::string user;
    bool visible;
};
    
//typedef boost::tuple<int64,int64,int64,int64,std::string> info;

struct member {
    member() : type(0),ref(0),role("") {}
    member(uint64 t, int64 r, std::string rl) : type(t),ref(r),role(rl) {}
    uint64 type;
    int64 ref;
    std::string role;
};
//typedef boost::tuple<uint64,int64,std::string> member;
typedef std::vector<tag> tagvector;
typedef std::vector<int64> refvector;
typedef std::vector<member> memvector;


inline void stopIteration() {
    
    PyErr_SetString(PyExc_StopIteration,"done iterating");
    throw boost::python::error_already_set();
    
}
class element {
    public:
        element(int64 t, uint64 c, int64 i, int64 q, info inf, std::vector<tag> tags)
            : type_(t),changetype_(c), id_(i), quadtree_(q), info_(inf), tags_(tags) {}
        uint64 Type() { return type_; }
        uint64 ChangeType() { return changetype_; }
        int64 Id() { return id_; }
        info Info()  { return info_; }
        const tagvector& Tags() const { return tags_; } 
        int64 Quadtree() { return quadtree_; }
        
        virtual ~element() {}
        
    private:
        uint64 type_,changetype_ ;
        int64 id_, quadtree_ ;
        info info_;
        tagvector tags_;
};
        
class node : public element {
    public:
        node(uint64 c, int64 i, int64 q, info inf, std::vector<tag> tags, int64 lon, int64 lat)
            : element(0,c,i,q,inf,tags), lon_(lon), lat_(lat) {}
        
        virtual ~node() {}
        
        int64 Lon() { return lon_; }
        int64 Lat() { return lat_; }
    private:
        int64 lon_,lat_;
};

class way : public element {
    public:
        way(uint64 c, int64 i, int64 q, info inf, tagvector tags, refvector refs)
            : element(1,c,i,q,inf,tags), refs_(refs) {}
        
        virtual ~way() {}
        
        const refvector& Refs() const { return refs_; }
    private:
        refvector refs_;
};

class relation : public element {
    public:
        relation(uint64 c, int64 i, int64 q, info inf, tagvector tags, memvector mems)
            : element(2,c,i,q,inf,tags), mems_(mems) {}
        
        virtual ~relation() {}
        
        const memvector&  Members() const { return mems_; }
    private:
        memvector mems_;
};
    
class geometryelement : public element {
    public:
        geometryelement(uint64 c, int64 i, int64 q, info inf, std::vector<tag> tags, std::shared_ptr<geometry> geom)
            : element(2,c,i,q,inf,tags), geom_(geom) {}
        
        virtual ~geometryelement() {}
    
        std::shared_ptr<geometry>    Geometry() { return geom_; }
    private:
        std::shared_ptr<geometry> geom_;
};
struct primitiveblock {
    primitiveblock(int64 i) : idx(i),quadtree(-1),startdate(0),enddate(0) {
        objects.reserve(8000);
    }
    
    int64 idx;
    std::vector<std::shared_ptr<element> > objects;
    int64 quadtree;
    int64 startdate, enddate;
    tagvector tags;
    
        
};

#endif

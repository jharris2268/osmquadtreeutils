#include "simplepbf.hpp"
#include <boost/python.hpp>
#include <iostream>

    



std::shared_ptr<element> readNode(const std::string& data, const std::vector<std::string>& stringtable,
    uint64 ct);
    
    
int readDenseNodes(const std::string& data, const std::vector<std::string>& stringtable, 
    uint64 ct, std::vector<std::shared_ptr<element> >& objects);


std::shared_ptr<element> readWay(const std::string& data, const std::vector<std::string>& stringtable,
    uint64 ct);
std::shared_ptr<element> readRelation(const std::string& data, const std::vector<std::string>& stringtable,
    uint64 ct);

std::shared_ptr<element> readGeometry(const std::string& data, const std::vector<std::string>& stringtable,
    uint64 ct);



void readPrimitiveGroupCommon(
        const std::string& data, const std::vector<std::string>& stringtable,
        std::vector<std::shared_ptr<element> >& objects,
        uint64 ct) {
    
    //std::cout << "readPrimitiveGroupCommon(" << data.size() << " bytes, " << stringtable.size() << " strings, " << objects.size() << " objects, " << ct << ");" << std::endl;
    
    
    size_t pos=0;
    uint64 a,b;
    std::string c;
    
    //std::vector<std::shared_ptr<element> > objects;
    
    boost::tie(a,b,c) = readPbfTag(data,pos);
    for ( ; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        
        if (a==1) {
            objects.push_back(readNode(c, stringtable,ct));
        } else if (a==2) {
            readDenseNodes(c, stringtable, ct,objects);
        } else if (a==3) {
            objects.push_back(readWay(c, stringtable,ct));
        } else if (a==4) {
            objects.push_back(readRelation(c, stringtable,ct));
        } else if (a==20) {
            objects.push_back(readGeometry(c, stringtable,ct));
        }
        //std::cout << a << " " << c.size() << " [" << objects.size() << "]" << std::endl;
    }
    
    return;
}

void  readPrimitiveGroup(const std::string& data, const std::vector<std::string>& stringtable, std::vector<std::shared_ptr<element> >& objects, bool change) {
    
    //std::cout << "readPrimitiveGroup(" << data.size() << " bytes, " << stringtable.size() << " strings, " << objects.size() << " objects, " << change << ");" << std::endl;
    
    if (!change) {
        return readPrimitiveGroupCommon(data,stringtable,objects,0);
    }
    
    size_t pos=0;
    uint64 a,b;
    std::string c;
    uint64 ct=0;
    
    boost::tie(a,b,c) = readPbfTag(data,pos);
    for ( ; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==10) { ct=b; }
    }
    return readPrimitiveGroupCommon(data,stringtable,objects, ct);
}

info readInfo(const std::string& data,const std::vector<std::string>& stringtable) {
    
    info ans;    
    size_t pos = 0;
    uint64 a,b; std::string c;
    boost::tie(a,b,c) = readPbfTag(data,pos);
    
    bool vv=true;
    
    for ( ; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) { ans.version = int64(b); }
        else if (a==2) { ans.timestamp = int64(b); }
        else if (a==3) { ans.changeset = int64(b); }
        else if (a==4) { ans.user_id = int64(b); }
        else if (a==5) { ans.user = stringtable.at(b); }
        else if (a==6) { vv = b!=0; }
        
    }
    if (ans.version!=0) {
        ans.visible=vv;
    }
    return ans;
    /*
    if (objectFactory.has_key("info")) {
        return objectFactory["info"](vs,cs,ts,ui,stringtable.at(us));
    }
    return boost::python::object(boost::python::make_tuple(vs,cs,ts,ui,stringtable.at(us)));*/
}

tagvector makeTags(
    const std::vector<uint64>& keys, const std::vector<uint64>& vals,
    const std::vector<std::string>& stringtable) {
    
    tagvector ans(keys.size());
    for (size_t i = 0; i < keys.size(); i++) {
        ans[i].key=stringtable.at(keys.at(i));
        ans[i].val=stringtable.at(vals.at(i));
    }
    return ans;
    
    /*
        if (objectFactory.has_key("tag")) {
            ans.append(objectFactory["tag"](k,v));
        } else {
            ans.append(boost::python::make_tuple(k,v));
        }
    }
    
    return boost::python::object(ans);*/
}
    
    
//boost::tuple<int64,boost::python::object,boost::python::object,int64,std::vector<PbfTag> >
boost::tuple<int64,info,tagvector,int64,std::vector<PbfTag> >
    readCommon(const std::string& data, const std::vector<std::string>& stringtable) {

    int64 id=0, quadtree=0;
    info info;
    tagvector tags;
    std::vector<PbfTag> rem;
    
    std::vector<uint64> keys,vals;
    
    size_t pos = 0;
    uint64 a,b; std::string c;
    boost::tie(a,b,c) = readPbfTag(data,pos);
    for ( ; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) {
            id = int64(b);
        } else if (a==2) {
            keys = readPackedInt(c);
        } else if (a==3) {
            vals = readPackedInt(c);
        } else if (a==4) {
            info = readInfo(c,stringtable);
        } else if (a==20) {
            quadtree = unZigZag(b);
        } else {
            rem.push_back(PbfTag(a,b,c));
        }
    }
    
    tags = makeTags(keys,vals,stringtable);
    return boost::make_tuple(id,info,tags,quadtree,rem);
}
    

std::shared_ptr<element> readNode(
        const std::string& data, const std::vector<std::string>& stringtable,
        uint64 ct) {
    int64 id,qt;
    //boost::python::object info,tags;
    info inf; tagvector tags;
    std::vector<PbfTag> rem;
    
    boost::tie(id,inf,tags,qt,rem) = readCommon(data,stringtable);
    
    int64 lon=0,lat=0;
    for (size_t i=0; i < rem.size(); i++) {
        if (rem[i].get<0>() == 8) {
            lat = unZigZag(rem[i].get<1>());
        } else if (rem[i].get<0>() == 9) {
            lon = unZigZag(rem[i].get<1>());
        }
    }
    return std::make_shared<node>(ct,id,qt,inf,tags, lon,lat);
    /*
    if (objectFactory.has_key("node")) {
        return objectFactory["node"](id,info,tags,lon,lat,qt,ct);
    }
    
    
    boost::python::object lonlat(boost::python::make_tuple(lon,lat));
    
    return boost::python::object(boost::python::make_tuple(0,id,info,tags,lonlat,qt,ct));*/
}
    
boost::tuple<std::vector<uint64>,std::vector<int64>,std::vector<int64>,std::vector<int64>,std::vector<int64>,std::vector<uint64> >
    readDenseInfo(const std::string& data)
{
    std::vector<int64> ts,cs,ui,us;
    std::vector<uint64> vs, vv;
    
    uint64 a,b; std::string c;
    size_t pos = 0;
    boost::tie(a,b,c) = readPbfTag(data,pos);
    
    for ( ; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) { vs = readPackedInt(c); }
        if (a==2) { ts = readPackedDelta(c); }
        if (a==3) { cs = readPackedDelta(c); }
        if (a==4) { ui = readPackedDelta(c); }
        if (a==5) { us = readPackedDelta(c); }
        if (a==6) { vv = readPackedInt(c); }
    }
    return boost::make_tuple(vs,ts,cs,ui,us,vv);
}
    
    
int readDenseNodes(
        const std::string& data, const std::vector<std::string>& stringtable,
        uint64 ct, std::vector<std::shared_ptr<element> >& objects) {

    std::vector<int64> ids, lons, lats, qts;
    std::vector<uint64> kvs;
    
    std::vector<int64> ts,cs,ui,us;
    std::vector<uint64> vs,vv;
    
    uint64 a,b; std::string c;
    size_t pos = 0;
    boost::tie(a,b,c) = readPbfTag(data,pos);
    
    for ( ; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) { ids = readPackedDelta(c); }
        else if (a==5) { boost::tie(vs,ts,cs,ui,us,vv) = readDenseInfo(c); }
        else if (a==8) { lats = readPackedDelta(c); }
        else if (a==9) { lons = readPackedDelta(c); }
        else if (a==10) { kvs = readPackedInt(c); }
        else if (a==20) { qts = readPackedDelta(c); }
    }
    
    size_t tagi = 0;
    
    for (size_t i = 0; i < ids.size(); i++) {
        int64 id = ids.at(i);
        tagvector tags;
        info inf;
        
        if (i<vs.size()) {
            inf = info( //boost::python::object(boost::python::make_tuple(
                vs.at(i),
                ts.at(i),
                cs.at(i),
                ui.at(i),
                stringtable.at(us.at(i))
            );
            if (vv.size()>0) {
                inf.visible = vv.at(i)!=0;
            }
        }
        if (tagi < kvs.size()) {
            //boost::python::list tt;
            while ((tagi < kvs.size()) && kvs.at(tagi)!=0) {
                //tt.append(boost::python::make_tuple(
                tags.push_back(tag(
                    stringtable.at(kvs.at(tagi)),
                    stringtable.at(kvs.at(tagi+1))
                ));
                tagi+=2;
            }
            tagi += 1;
            //tags = boost::python::object(tt);
        }
        
        int64 qt = -1;
        
        //boost::python::object qt;
        if (i < qts.size()) {
            //qt = boost::python::object(qts.at(i));
            qt=qts.at(i);
        }
        int64 lon=0,lat = 0;
        if (i < lons.size()) {
            lon=lons.at(i);
            lat=lats.at(i);
        }
        objects.push_back(std::make_shared<node>(ct,id,qt,inf,tags,lon,lat));
        /*
        if (objectFactory.has_key("node")) {
            objects.append(objectFactory["node"](id,info,tags,lons.at(i), lats.at(i),qt,ct));
        } else {
            boost::python::tuple lonlat = boost::python::make_tuple(
                lons.at(i), lats.at(i));
            objects.append(boost::python::make_tuple(
                0, id, info, tags, lonlat, qt,ct));
        }*/
    }
    return ids.size();
}

std::shared_ptr<element> readWay(const std::string& data, const std::vector<std::string>& stringtable, uint64 ct) {
    
    int64 id,qt;
    //boost::python::object info,tags;
    info inf; tagvector tags;
    std::vector<PbfTag> rem;
    
    boost::tie(id,inf,tags,qt,rem) = readCommon(data,stringtable);
    
    std::vector<int64> refs;
    for (size_t i=0; i < rem.size(); i++) {
        if (rem[i].get<0>() == 8) {
            refs = readPackedDelta(rem[i].get<2>());
            
        }
    }
    return std::make_shared<way>(ct,id,qt,inf,tags,refs);
    /*
    if (objectFactory.has_key("way")) {
        return objectFactory["way"](id,info,tags,waynds,qt,ct);
    }
    
    return boost::python::object(boost::python::make_tuple(1,id,info,tags,waynds,qt,ct));*/
}

std::shared_ptr<element> readRelation(const std::string& data, const std::vector<std::string>& stringtable,
        uint64 ct) {
    int64 id,qt;
    //boost::python::object info,tags;
    info inf; tagvector tags;
    std::vector<PbfTag> rem;
    
    boost::tie(id,inf,tags,qt,rem) = readCommon(data,stringtable);
    
    std::vector<uint64> ty, rl;
    std::vector<int64> rf;
    for (size_t i=0; i < rem.size(); i++) {
        if (rem[i].get<0>() == 8) {
            rl = readPackedInt(rem[i].get<2>());
        } else if (rem[i].get<0>() == 9) {
            rf = readPackedDelta(rem[i].get<2>());
        } else if (rem[i].get<0>() == 10) {
            ty = readPackedInt(rem[i].get<2>());
        } 
    }
    memvector mems(rf.size());
    for (size_t i=0; i < rf.size(); i++) {
        mems[i].type = ty[i];
        mems[i].ref = rf[i];
        if (i < rl.size()) {
            mems[i].role = stringtable.at(rl[i]);
        }
    }
    
    return std::make_shared<relation>(ct,id,qt,inf,tags,mems);
    
    /*
    boost::python::list mems;
    
    for (size_t j=0; j < rf.size(); j++) {
        if (objectFactory.has_key("mem")) {
            mems.append(objectFactory["mem"](ty.at(j),rf.at(j),stringtable.at(rl.at(j))));
        } else {
            mems.append(boost::python::make_tuple(ty.at(j),rf.at(j),stringtable.at(rl.at(j))));
        }
    }
    if (objectFactory.has_key("relation")) {
        return objectFactory["relation"](id,info,tags,mems,qt,ct);
    }
    
    return boost::python::object(boost::python::make_tuple(2,id,info,tags,mems,qt,ct));*/
}

std::shared_ptr<geometry> makeGeometry(const std::vector<PbfTag>& msgs);

std::shared_ptr<element> readGeometry(const std::string& data, const std::vector<std::string>& stringtable, uint64 ct) {
    int64 id,qt;
    //boost::python::object info,tags;
    info inf; tagvector tags;
    std::vector<PbfTag> rem;
    
    boost::tie(id,inf,tags,qt,rem) = readCommon(data,stringtable);
    
    std::shared_ptr<geometry> geom = makeGeometry(rem);
    
    return std::make_shared<geometryelement>(ct,id,qt,inf,tags,geom);
    
    /*
    boost::python::list mems;
    
    for (size_t j=0; j < rf.size(); j++) {
        if (objectFactory.has_key("mem")) {
            mems.append(objectFactory["mem"](ty.at(j),rf.at(j),stringtable.at(rl.at(j))));
        } else {
            mems.append(boost::python::make_tuple(ty.at(j),rf.at(j),stringtable.at(rl.at(j))));
        }
    }
    if (objectFactory.has_key("relation")) {
        return objectFactory["relation"](id,info,tags,mems,qt,ct);
    }
    
    return boost::python::object(boost::python::make_tuple(2,id,info,tags,mems,qt,ct));*/
}
        
boost::python::object get_object(std::shared_ptr<element> ele) {
    std::shared_ptr<node> nd = std::dynamic_pointer_cast<node>(ele);
    if (nd) {
        return boost::python::object(nd);
    }
    
    std::shared_ptr<way> wy = std::dynamic_pointer_cast<way>(ele);
    if (wy) {
        return boost::python::object(wy);
    }
    
    std::shared_ptr<relation> rl = std::dynamic_pointer_cast<relation>(ele);
    if (rl) {
        return boost::python::object(rl);
    }
    
    std::shared_ptr<geometryelement> gg = std::dynamic_pointer_cast<geometryelement>(ele);
    if (gg) {
        return boost::python::object(gg);
    }
    return boost::python::object();
}

boost::python::object get_object_at(const std::vector<std::shared_ptr<element> >& bl, int i) {
    if (i>=bl.size()) {
        stopIteration();
    }
    return get_object(bl.at(i));
}

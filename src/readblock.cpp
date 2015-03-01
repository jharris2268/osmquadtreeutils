#include "simplepbf.hpp"

#include <Python.h>
#include <boost/python.hpp>


boost::python::object get_object_at(const std::vector<std::shared_ptr<element> >& bl, int i);
//boost::python::object make_tags_list(tagvector tags);




std::vector<std::string> readStringTable(const std::string& data);
int64 readQuadTree(const std::string& data);
void readPrimitiveGroup(const std::string& data, const std::vector<std::string>& stringtable, std::vector<std::shared_ptr<element> >& objects, bool change);

tagvector makeTags(
    const std::vector<uint64>& keys, const std::vector<uint64>& vals,
    const std::vector<std::string>& stringtable);

std::shared_ptr<primitiveblock> readPrimitiveBlock(int64 idx, const std::string& data, bool change) {
    
    
    std::vector<std::string> stringtable;
    
    std::shared_ptr<primitiveblock> primblock(new primitiveblock(idx));
    
    
    std::vector<uint64> kk,vv;
    
    uint64 a,b;
    std::string c;
    size_t pos=0;
    
    std::vector<std::string> blocks;
    boost::tie(a,b,c) = readPbfTag(data,pos);
    
    for ( ; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) {
            stringtable = readStringTable(c);
        } else if (a==2) {
            blocks.push_back(c);
        } else if (a==31) {
            primblock->quadtree = readQuadTree(c);
        } else if (a==33) {
            primblock->startdate = int64(b);
        } else if (a==34) {
            primblock->enddate = int64(b);
        } else if (a==35) {
            kk = readPackedInt(c);
        } else if (a==36) {
            vv = readPackedInt(c);
        }
    }
    

    if (kk.size()>0) {
        //printf("blocktags %d %d\n",kk.size(), vv.size());
        primblock->tags = makeTags(kk,vv,stringtable);
    }
    
    
    for (size_t i=0; i < blocks.size(); i++) {
        readPrimitiveGroup(blocks[i], stringtable, primblock->objects,change);
    }
    
    /*
    if (objectFactory.has_key("block")) {
        return objectFactory["block"](idx,quadtree,startdate,enddate,objects,tags);
    }
    
    return boost::python::object(boost::python::make_tuple(idx,quadtree,startdate,enddate,objects,tags));*/
    return primblock;
}
        
std::vector<std::string> readStringTable(const std::string& data) {
    uint64 a,b;
    std::string c;
    size_t pos=0;
    
    std::vector<std::string> result;
    boost::tie(a,b,c) = readPbfTag(data,pos);
    for ( ; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) {
            result.push_back(c);
        }
    }
    return result;
}

int64 readQuadTree(const std::string& data) {
    uint64 x=0,y=0,z=0;
    uint64 a,b;
    std::string c;
    size_t pos=0;
    boost::tie(a,b,c) = readPbfTag(data,pos);
    for ( ; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) { x=b; }
        if (a==2) { y=b; }
        if (a==3) { z=b; }
    }
    
    int64 ans=0;
    int64 scale = 1;
    
    for (uint64 i=0; i < z; i++) {
        ans += (int64((x>>i)&1) | int64(((y>>i)&1)<<1) ) * scale;
		scale *= 4;
	}

	ans <<= (63 - (2 * z));
	ans |= int64(z);
	return ans;
}
    
boost::python::object tag_list(const tagvector& t) {
    boost::python::list ll;
    for (size_t i=0; i < t.size(); i++) {
        ll.append(boost::python::make_tuple(t[i].key,t[i].val));
    }
    return ll;
}

boost::python::object pb_tag_list(std::shared_ptr<primitiveblock> pb) {
    return tag_list(pb->tags);
}

template <class T>
boost::python::object element_tag_list(std::shared_ptr<T> ele) {
    return tag_list(ele->Tags());
}

boost::python::object way_refs(std::shared_ptr<way> wy) {
    boost::python::list ll;
    const refvector& t = wy->Refs();
    for (size_t i=0; i < t.size(); i++) {
        ll.append(t[i]);
    }
    return ll;
}
    
 /*   
struct refvector_to_python {
    static PyObject* convert(refvector const& t) {
        boost::python::list ll;
        for (size_t i=0; i < t.size(); i++) {
            ll.append(t[i]);
        }
        return boost::python::incref(ll.ptr());
    }
};
*/

boost::python::object relation_members(std::shared_ptr<relation> rel) {
    boost::python::list ll;
    const memvector& t = rel->Members();
    for (size_t i=0; i < t.size(); i++) {
            ll.append(boost::python::make_tuple(t[i].type,t[i].ref,t[i].role));
        }
    return ll;
}
/*
struct memvector_to_python {
    static PyObject* convert(memvector const& t) {
        boost::python::list ll;
        for (size_t i=0; i < t.size(); i++) {
            ll.append(boost::python::make_tuple(t[i].type,t[i].ref,t[i].role));
        }
        return boost::python::incref(ll.ptr());
    }
};*/


boost::python::object info_tuple(const info& inf) {
    return boost::python::make_tuple(
        inf.version,inf.timestamp,inf.changeset,inf.user_id,inf.user);
}


boost::python::object node_tuple(std::shared_ptr<node> nd) {
    return boost::python::make_tuple(0,
        nd->Id(), info_tuple(nd->Info()), element_tag_list(nd),
        nd->Lon(),nd->Lat(),nd->Quadtree(), nd->ChangeType());
}

boost::python::object way_tuple(std::shared_ptr<way> wy) {
    return boost::python::make_tuple(1,
        wy->Id(), info_tuple(wy->Info()), element_tag_list(wy),
        way_refs(wy),wy->Quadtree(), wy->ChangeType());
}

boost::python::object relation_tuple(std::shared_ptr<relation> rl) {
    return boost::python::make_tuple(2,
        rl->Id(), info_tuple(rl->Info()), element_tag_list(rl),
        relation_members(rl),rl->Quadtree(), rl->ChangeType());
}

boost::python::object get_geometry_tuple(std::shared_ptr<geometryelement> gg);

boost::python::object geometry_tuple(std::shared_ptr<geometryelement> gg) {
        
    return boost::python::make_tuple(3,
        gg->Id(), info_tuple(gg->Info()), element_tag_list(gg),
        get_geometry_tuple(gg),gg->Quadtree(), gg->ChangeType());
}
boost::python::object get_geometry(std::shared_ptr<geometryelement> geom);

using namespace boost::python;
void init_primitve_block() {
    def("readPrimitiveBlock",&readPrimitiveBlock);
    
    
    //to_python_converter<tagvector, tagvector_to_python>();
    //to_python_converter<refvector, refvector_to_python>();
    //to_python_converter<memvector, memvector_to_python>();
    
    class_<primitiveblock, std::shared_ptr<primitiveblock>, boost::noncopyable>("PrimitiveBlock",no_init)
        .def_readwrite("Idx", &primitiveblock::idx)
        .def_readonly("Objects", &primitiveblock::objects)
        .def_readonly("Quadtree", &primitiveblock::quadtree)
        .def_readonly("StartDate", &primitiveblock::startdate)
        .def_readonly("EndDate", &primitiveblock::enddate)
        //.def_readonly("Tags", pb_tag_list)
        ;
    
    class_<info>("Info",no_init)
        .add_property("Version", &info::version)
        .add_property("Timestamp", &info::timestamp)
        .add_property("Changeset", &info::changeset)
        .add_property("UserId", &info::user_id)
        .add_property("User", &info::user)
        .add_property("tuple", info_tuple)
    ;
    
    class_<std::vector<std::shared_ptr<element> >, boost::noncopyable>("ElementList",no_init)
        .def("__len__", &std::vector<std::shared_ptr<element> >::size)
        .def("__getitem__", get_object_at)
    ;
    
    class_<element, std::shared_ptr<element>, boost::noncopyable>("Element", no_init)
        .add_property("Type", &element::Type)
        .add_property("ChangeType", &element::ChangeType)
        .add_property("Id", &element::Id)
        .add_property("Info", &element::Info)
        
        .add_property("Quadtree", &element::Quadtree)
        
        
    ;
    
    class_<node, std::shared_ptr<node>, bases<element>, boost::noncopyable>("Node",no_init)
        .add_property("Lon", &node::Lon)
        .add_property("Lat", &node::Lat)
        .add_property("tuple", node_tuple)
        
        .add_property("Tags", element_tag_list<node>)
    ;
    
    class_<way, std::shared_ptr<way>, bases<element>, boost::noncopyable>("Way",no_init)
        .add_property("Refs", way_refs)
        .add_property("tuple", way_tuple)
        
        .add_property("Tags", element_tag_list<way>)
    ;
    
    class_<relation, std::shared_ptr<relation>, bases<element>, boost::noncopyable>("Relation",no_init)
        .add_property("Members", relation_members)
        .add_property("tuple", relation_tuple)
        
        .add_property("Tags", element_tag_list<relation>)
    ;
    class_<geometryelement, std::shared_ptr<geometryelement>, bases<element>, boost::noncopyable>("GeometryElement",no_init)
        .add_property("Geometry", get_geometry)
        .add_property("tuple", geometry_tuple)
        
        .add_property("Tags", element_tag_list<geometryelement>)
    ;
    
    //class_<geometry, std::shared_ptr<geometry>, boost::noncopyable>("Geometry",no_init);
    
}


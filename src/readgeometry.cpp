#include "simplepbf.hpp"
#include <boost/python.hpp>


#include "readgeometry.hpp"


boost::tuple<int64,info,tagvector,int64,std::vector<PbfTag> >
    readCommon(const std::string& data, const std::vector<std::string>& stringtable);

point readPoint(const std::string& data) {
    uint64 a,b; std::string c;
    size_t pos=0;
    point ans;
    
    boost::tie(a,b,c) = readPbfTag(data,pos);
    for (; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) { ans.ref = unZigZag(b); }
        if (a==2) { ans.lon = unZigZag(b); }
        if (a==3) { ans.lat = unZigZag(b); }
    }
    return ans;
}

Ring readRing(const std::string& data) {
    uint64 a,b; std::string c;
    size_t pos=0;
    
    std::vector<int64> ref,lon,lat;
    
    boost::tie(a,b,c) = readPbfTag(data,pos);
    for (; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) { ref = readPackedDelta(c); }
        if (a==2) { lon = readPackedDelta(c); }
        if (a==3) { lat = readPackedDelta(c); }
    }
    
    Ring ans(ref.size());
    for (size_t i=0; i < ref.size(); i++) {
        ans[i].ref = ref[i];
        ans[i].lon = lon[i];
        ans[i].lat = lat[i];
    }
    
    return ans;
}
        
Poly readPoly(const std::string& data) {
    uint64 a,b; std::string c;
    size_t pos=0;
    Poly ans;
    
    boost::tie(a,b,c) = readPbfTag(data,pos);
    for (; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) { ans.push_back(readRing(c));  }
    }
    return ans;
}


bbox make_bbox(const Ring& r) {
    bbox ans;
    for (size_t i=0; i < r.size(); i++) {
        if (r[i].lon < ans.minx) { ans.minx=r[i].lon; }
        if (r[i].lat < ans.miny) { ans.miny=r[i].lat; }
        if (r[i].lon > ans.maxx) { ans.maxx=r[i].lon; }
        if (r[i].lat > ans.maxy) { ans.maxy=r[i].lat; }
    }
    return ans;
}

bbox readGeoBBox(const std::string& data) {
    
    uint64 a,b; std::string c;
    size_t pos=0;
    
    bbox bx;
    
    boost::tie(a,b,c) = readPbfTag(data,pos);
    for ( ; a>0; boost::tie(a,b,c) = readPbfTag(data,pos)) {
        if (a==1) { bx.minx = unZigZag(b); }
        if (a==2) { bx.miny = unZigZag(b); }
        if (a==5) { bx.maxx = unZigZag(b); }
        if (a==6) { bx.maxy = unZigZag(b); }
    }
    
    return bx;
}
/*    
boost::python::object makeGeoBBoxPt(const Point& pt, boost::python::dict objectFactory) {
    if (objectFactory.has_key("bbox")) {
        return objectFactory["bbox"](pt.get<1>(),pt.get<2>(),pt.get<1>(),pt.get<2>());
    }
    
    return boost::python::make_tuple(pt.get<1>(),pt.get<2>(),pt.get<1>(),pt.get<2>());
}*/


std::shared_ptr<geometry> makeGeometry(const std::vector<PbfTag>& rem) {
    bbox bx;
        
    uint64 gt=0;
    
    int64 zo=0;
    double ar=0.0;
    std::vector<point> pts;
    std::vector<Ring> lns;
    std::vector<Poly> pls;
    
    uint64 a,b; std::string c;
    for (size_t i=0; i < rem.size(); i++) {
        boost::tie(a,b,c) = rem[i];
        
        if (a==10) { gt = b; }
        if (a==11) { zo = unZigZag(b); }
        if (a==12) { ar = toDouble(b); }
        if (a==13) { pts.push_back(readPoint(c)); };
        if (a==14) { lns.push_back(readRing(c)); };
        if (a==15) { pls.push_back(readPoly(c)); };
        if (a==16) { bx = readGeoBBox(c); }
    }
    
    
    if ((gt==1) && (pts.size()==1)) {
        return std::make_shared<pointgeometry>(pts[0]);
    } else if ((gt==2) && (lns.size()==1)) {
        if (bx.minx == 1800000000) {
            bx=make_bbox(lns[0]);
        }
        return std::make_shared<linegeometry>(lns[0], zo, bx);
    } else if ((gt==3) && (pls.size()==1)) {
        if (bx.minx == 1800000000) {
            bx=make_bbox(pls[0][0]);
        }
        return std::make_shared<polygongeometry>(pls[0], zo, ar, bx);
    } else if ((gt==7) && (pls.size()>1)) {
        if (bx.minx == 1800000000) {
            for (size_t z=0; z< pls.size(); z++) {
                bx=make_bbox(pls[z][0]);
            }
        }
        return std::make_shared<multigeometry>(pls, zo, ar, bx);
    }/* else if ((gt==7) && ((lns.size()>1 || (pls.size()>1)))) {
        geom = makeMulti(lns,pls,zo,ar,objectFactory);
    }
    
    if (objectFactory.has_key("geometry")) {
        return objectFactory["geometry"](id,info,tags,geom,qt,ct,bbox);
    }
    
    return boost::python::object(boost::python::make_tuple(
        3,id,info,tags,geom,qt,ct,bbox));*/
    
    return std::shared_ptr<geometry>();
    
}

boost::python::object bbox_tuple(const bbox& bb) {
    return boost::python::make_tuple(bb.minx*0.0000001,bb.miny*0.0000001,bb.maxx*0.0000001,bb.maxy*0.0000001);
}

boost::python::object point_tuple(const point& p) {
    return boost::python::make_tuple(p.ref,p.lon,p.lat);
}

boost::python::object get_geometry(std::shared_ptr<geometryelement> geom) {
    std::shared_ptr<pointgeometry> pt = std::dynamic_pointer_cast<pointgeometry>(geom->Geometry());
    if (pt) {
        return boost::python::object(pt);
    }
    
    std::shared_ptr<linegeometry> ln = std::dynamic_pointer_cast<linegeometry>(geom->Geometry());
    if (ln) {
        return boost::python::object(ln);
    }
    
    std::shared_ptr<polygongeometry> py = std::dynamic_pointer_cast<polygongeometry>(geom->Geometry());
    if (py) {
        return boost::python::object(py);
    }
    
    std::shared_ptr<multigeometry> mg = std::dynamic_pointer_cast<multigeometry>(geom->Geometry());
    if (mg) {
        return boost::python::object(mg);
    }
    
    return boost::python::object();
}

boost::python::object get_geometry_tuple(std::shared_ptr<geometryelement> geom) {
    if (true){
        std::shared_ptr<pointgeometry> pt = std::dynamic_pointer_cast<pointgeometry>(geom->Geometry());
        if (pt) {
            return boost::python::make_tuple(1, point_tuple(pt->Point()), bbox_tuple(pt->Bbox()));
        }
        
    }
    if (true){
        std::shared_ptr<linegeometry> ln = std::dynamic_pointer_cast<linegeometry>(geom->Geometry());
        if (ln) {
            boost::python::list ll;
            for (int i=0; i < ln->NumPoints(); i++) {
                ll.append(point_tuple(ln->Point(i)));
            }
            
            return boost::python::make_tuple(2, ll, ln->ZOrder(),bbox_tuple(ln->Bbox()));
            
        }
    }
    if (true) {
        std::shared_ptr<polygongeometry> py = std::dynamic_pointer_cast<polygongeometry>(geom->Geometry());
        if (py) {
            boost::python::list ll;
            for (int j=0; j < py->NumRings(); j++) {
                boost::python::list ss;
                for (int i=0; i < py->NumPoints(j); i++) {
                    ss.append(point_tuple(py->Point(j,i)));
                }
                ll.append(ss);
            }
            
            return boost::python::make_tuple(3, ll, py->ZOrder(),py->WayArea(),bbox_tuple(py->Bbox()));
        }
    }
    
    if (true) {
        std::shared_ptr<multigeometry> mg = std::dynamic_pointer_cast<multigeometry>(geom->Geometry());
        if (mg) {
            boost::python::list pp;
            for (int i=0; i < mg->NumGeometries(); i++) {
                boost::python::list ll;
                for (int j=0; j < mg->NumRings(i); j++) {
                    boost::python::list ss;
                    for (int k=0; k < mg->NumPoints(i,j); k++) {
                        ss.append(point_tuple(mg->Point(i,j,k)));
                    }
                    ll.append(ss);
                }
                pp.append(ll);
            }
            
            return boost::python::make_tuple(7,pp, mg->ZOrder(),mg->WayArea(),bbox_tuple(mg->Bbox()));
        }
    }
    return boost::python::object();
}



void init_geometry() {
    using namespace boost::python;
    
    class_<point>("point",no_init)
        .def_readonly("ref", &point::ref)
        .def_readonly("lon", &point::lon)
        .def_readonly("lat", &point::lat)
        .add_property("tuple",point_tuple)
    ;
    
    class_<bbox>("bbox",no_init)
        .def_readonly("minx",&bbox::minx)
        .def_readonly("miny",&bbox::miny)
        .def_readonly("maxx",&bbox::maxx)
        .def_readonly("maxy",&bbox::maxy)
        .add_property("tuple",bbox_tuple)
    ;
    
    class_<geometry, std::shared_ptr<geometry>, boost::noncopyable>("geometry", no_init)
        .def("Type", &geometry::Type)
        .def("Bbox", &geometry::Bbox)
        .add_property("tuple", get_geometry_tuple)
    ;
    
    class_<pointgeometry, bases<geometry>, std::shared_ptr<pointgeometry>, boost::noncopyable>("pointgeometry",no_init)
        .def("Point", &pointgeometry::Point)
    ;
    
    class_<linegeometry, bases<geometry>, std::shared_ptr<linegeometry>, boost::noncopyable>("linegeometry",no_init)
        .def("NumPoints", &linegeometry::NumPoints)
        .def("Point", &linegeometry::Point)
        .def("ZOrder",&linegeometry::ZOrder)
    ;
    
    class_<polygongeometry, bases<geometry>, std::shared_ptr<polygongeometry>, boost::noncopyable>("polygongeometry",no_init)
        .def("NumRings", &polygongeometry::NumRings)
        .def("NumPoints", &polygongeometry::NumPoints)
        .def("Point", &polygongeometry::Point)
        .def("ZOrder",&polygongeometry::ZOrder)
        .def("WayArea",&polygongeometry::WayArea)
    ;
    
    class_<multigeometry, bases<geometry>, std::shared_ptr<multigeometry>, boost::noncopyable>("multigeometry",no_init)
        .def("NumRings", &multigeometry::NumGeometries)
        .def("NumRings", &multigeometry::NumRings)
        .def("NumPoints", &multigeometry::NumPoints)
        .def("Point", &multigeometry::Point)
        .def("ZOrder",&multigeometry::ZOrder)
        .def("WayArea",&multigeometry::WayArea)
    ;
}

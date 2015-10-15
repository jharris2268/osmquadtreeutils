#include <mapnik/featureset.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/geometry.hpp>
#include <mapnik/feature_factory.hpp>
#include <mapnik/value_types.hpp>
#include <mapnik/unicode.hpp>
#include <mapnik/wkb.hpp>
#include <mapnik/make_unique.hpp>
//#include <mapnik/json/feature_generator.hpp>

#include <Python.h>
#include <boost/python.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <iomanip>

#include "simplepbf.hpp"
#include "readgeometry.hpp"

#include <math.h>

std::shared_ptr<primitiveblock> readPrimitiveBlock(int64 idx, const std::string& data, bool change);


struct xy {
    double x, y;
    xy(double x_, double y_) : x(x_), y(y_) {}
};

const double earth_half_circum = 20037508.3428;
double merc(double y) {
    return log(tan(M_PI*(1.0+y/90.0)/4.0)) * 90.0 / M_PI;
}

    
xy get_point(point pt, bool reproj) {
    
    double x = pt.lon*0.0000001;
    double y = pt.lat*0.0000001;
    
    if (reproj) {
        x *= earth_half_circum / 180.0;
        y = merc(y) * earth_half_circum / 90.0;
    }
    return xy(x,y);
}
    
    

int add_point_geom(mapnik::feature_ptr fs, std::shared_ptr<pointgeometry> pt, bool reproj) {
    
    
    /*auto gg = std::make_unique<mapnik::geometry_type>(mapnik::geometry_type::types::Point);
    
    xy p0 = get_point(pt->Point(), reproj);
    
    gg->move_to(p0.x, p0.y);
    
    fs->add_geometry(gg.release());
    
    
    return 1;*/
    
    xy p0 = get_point(pt->Point(), reproj);
    fs->set_geometry(mapnik::geometry::geometry<double>(mapnik::geometry::point<double>(p0.x,p0.y)));
    return 1;
}

int add_line_geom(mapnik::feature_ptr fs, std::shared_ptr<linegeometry> ln, bool reproj) {
    //auto gg = std::make_unique<mapnik::geometry_type>(mapnik::geometry_type::types::LineString);
    mapnik::geometry::line_string<double> ls;
    ls.reserve(ln->NumPoints());
    
    for (int i=0; i < ln->NumPoints(); i++) {
    
        xy p = get_point(ln->Point(i), reproj);
        ls.add_coord(p.x,p.y);
        /*if (i==0) {
            gg->move_to(p.x, p.y);
        } else {
            gg->line_to(p.x, p.y);
        }*/
    
    }
    
    //fs->add_geometry(gg.release());
    fs->set_geometry(mapnik::geometry::geometry<double>(ls));
    
    
    return 1;
}


int add_polygon_geom(mapnik::feature_ptr fs, std::shared_ptr<polygongeometry> py, bool reproj) {
    //auto gg = std::make_unique<mapnik::geometry_type>(mapnik::geometry_type::types::Polygon);
    mapnik::geometry::polygon<double> poly;
    
    
    for (int i=0; i < py->NumRings(); i++) {
        mapnik::geometry::line_string<double> ls;
        ls.reserve(py->NumPoints(i));
        for (int j=0; j < py->NumPoints(i); j++) {
            xy p = get_point(py->Point(i,j), reproj);
            ls.add_coord(p.x,p.y);
            /*if (j==0) {
                gg->move_to(p.x, p.y);
            } else {
                gg->line_to(p.x, p.y);
            }*/
            
        }
        if (i==0 ) {
            poly.set_exterior_ring(mapnik::geometry::linear_ring<double>(ls));
        } else {
            poly.add_hole(mapnik::geometry::linear_ring<double>(ls));
        }
        //gg->close_path();
    }
    
    //fs->add_geometry(gg.release());
    fs->set_geometry(mapnik::geometry::geometry<double>(poly));
    
    
    return 1;
}


int add_multi_geom(mapnik::feature_ptr fs, std::shared_ptr<multigeometry> mg, bool reproj) {
    
    int cc = 0;
    
    mapnik::geometry::geometry_collection<double> gc;
    
    for (int i=0; i < mg->NumGeometries(); i++) {
        
        
        //auto gg = std::make_unique<mapnik::geometry_type>(mapnik::geometry_type::types::Polygon);
        mapnik::geometry::polygon<double> py;
        for (int j=0; j < mg->NumRings(i); j++) {
            mapnik::geometry::line_string<double> ls;
            ls.reserve(mg->NumPoints(i,j));
            for (int k=0; k < mg->NumPoints(i,j); k++) {
                
                xy p = get_point(mg->Point(i,j,k), reproj);
                ls.add_coord(p.x,p.y);
                /*if (k==0) {
                    gg->move_to(p.x, p.y);
                } else {
                    gg->line_to(p.x, p.y);
                }*/
                
            }
            //gg->close_path();
            if (j==0) {
                py.set_exterior_ring(mapnik::geometry::linear_ring<double>(ls));
            } else {
                py.add_hole(mapnik::geometry::linear_ring<double>(ls));
            }
        }
        //fs->add_geometry(gg.release());
        gc.push_back(py);
        cc++;
    }
    
    fs->set_geometry(gc);
    
    return cc;
}



int add_geoms(mapnik::feature_ptr feat, std::shared_ptr<geometry> geom, bool reproj) {
    if (geom->Type()==1) {
        std::shared_ptr<pointgeometry> pt = std::dynamic_pointer_cast<pointgeometry>(geom);
        if (!pt) {
            return 0;
        }
        
        return add_point_geom(feat,pt,reproj);
    }
    
    if (geom->Type()==2) {
        std::shared_ptr<linegeometry> ln = std::dynamic_pointer_cast<linegeometry>(geom);
        if (!ln) {
            return 0;
        }
        
        return add_line_geom(feat,ln,reproj);
    }
    
    if (geom->Type()==3) {
        std::shared_ptr<polygongeometry> py = std::dynamic_pointer_cast<polygongeometry>(geom);
        if (!py) {
            return 0;
        }
        
        return add_polygon_geom(feat,py,reproj);
    }
    
    if (geom->Type()==7) {
        std::shared_ptr<multigeometry> mg = std::dynamic_pointer_cast<multigeometry>(geom);
        if (!mg) {
            return 0;
        }
        
        return add_multi_geom(feat,mg,reproj);
    }
    
    return 0;
}


int64 get_int_value(const std::string& c) {
    size_t p = 0;
    return readVarint(c,p);
}

mapnik::value_double get_double_value(const std::string& c) {
    size_t p = 0;
    uint64 u = readUVarint(c,p);
    return toDouble(u);
}

boost::tuple<size_t,std::string,std::string> get_tag(const tagvector& tg, int i) {
    if (tg[i].key.size() == 0) {
        std::cout << "NULL KEY " << i << " " << tg[i].val << std::endl;
        return boost::make_tuple(0,tg[i].key,tg[i].val);
    }
    char t = tg[i].key[0];
    std::string k1 = "";
    if (tg[i].key.size()>1) {
        k1 = tg[i].key.substr(1, std::string::npos);
    }
    
    if (t == '!') {
        return boost::make_tuple(1,k1,tg[i].val);
    }
    if (t == '%') {
        return boost::make_tuple(2,k1,tg[i].val);
    }
    if (t == '$') {
        return boost::make_tuple(3,k1,tg[i].val);
    }
    return boost::make_tuple(0,tg[i].key,tg[i].val);
}
        
    
mapnik::feature_ptr makePbfFeat(int id, std::shared_ptr<geometryelement> ge,
        std::map<std::string,size_t> keymap, mapnik::context_ptr ctx,
        const mapnik::transcoder& tr, bool reproj) {
    
    
    
    mapnik::feature_ptr fs = mapnik::feature_factory::create(ctx,id);   
  
    if (!add_geoms(fs, ge->Geometry(),reproj)) {
        return mapnik::feature_ptr();
    }
  
    for (size_t i = 0; i < ge->Tags().size(); i++) {
        std::string k,v;
        size_t t;
        boost::tie(t,k,v) = get_tag(ge->Tags(),i);
        if (keymap.count(k)) {
            if (t==0) {
                fs->put_new(k,tr.transcode(v.data(), v.length()));
            } else if (t==1) {
                std::stringstream ss;
                ss<<get_int_value(v);
                fs->put_new(k,tr.transcode(ss.str().data(),ss.str().length()));
            } else if (t==2) {
            fs->put_new(k,get_double_value(v));
            
            } else {
                fs->put_new(k,mapnik::value_null());
            }
            
        } 
    }
    //std::cout << std::endl;
    //std::cout << fs->to_string() << std::endl;
    return fs;
}
    




class MAPNIK_DECL MapnikPbfFeatureset : public mapnik::Featureset {
    public:
        MapnikPbfFeatureset(const std::string& indata, bool reproj) : 
            pos_(0), id_(0), reproj_(reproj),
            ctx_(new mapnik::context_type()), tr_("utf-8")
        {
            std::shared_ptr<primitiveblock> pb = readPrimitiveBlock(0,indata,false);
            if (!pb) {
                return;
            }
            const std::vector<std::shared_ptr<element> >& eles = pb->objects;
            
            for (size_t i = 0; i < eles.size(); i++) {
                std::shared_ptr<geometryelement> ge = 
                    std::dynamic_pointer_cast<geometryelement>(eles[i]);
                if (ge) {
                    objs.push_back(ge);
                }
            }
            
            std::vector<std::string> keys;
            tagvector pbt = pb->tags;
            //std::cout << "pb->tags:" << pbt.size() << " ";
            if (pbt.size()>0) {
                for (size_t i =0; i < pbt.size(); i++) {
                    //std::cout << pbt[i].key << " ";
                    if (pbt[i].key=="tags") {
                        //std::cout << pbt[i].val;
                        
                        boost::split(keys,pbt[i].val, boost::is_any_of(";"));
                    }
                }
            }
           // std::cout << std::endl;
                
           
            if ((objs.size()>0) && (keys.empty())) {
                const tagvector& tags = objs[0]->Tags();
                for (size_t i =0; i < tags.size(); i++) {
                    keys.push_back(get_tag(tags,i).get<1>());
                }
            }
            
            for (size_t i = 0; i < keys.size(); i++) {
                //std::cout << i << keys[i] << std::endl;
                keymap_[keys[i]]=i;
                ctx_->push(keys[i]);
            }
            
        }
        
        virtual mapnik::feature_ptr next() {
            if (id_ < objs.size()) {
                auto o= objs.at(id_);
                return makePbfFeat(id_++,o,keymap_,ctx_,tr_,reproj_);
            }
                        
            return mapnik::feature_ptr();
        }
        
        virtual ~MapnikPbfFeatureset() {}
    private:
        std::vector<std::shared_ptr<geometryelement> > objs;
        size_t pos_;
        int id_;
        bool reproj_;
        
        std::map<std::string,size_t> keymap_;
        mapnik::context_ptr ctx_;
        mapnik::transcoder tr_;
};

mapnik::featureset_ptr makeMapnikPbfFeatureset(const std::string& indata, bool rep) {
    return std::make_shared<MapnikPbfFeatureset>(indata,rep);
}

void init_primitve_block();
void init_geometry();

using namespace boost::python;
BOOST_PYTHON_MODULE(readblock)
{
    def("makeMapnikPbfFeatureset",&makeMapnikPbfFeatureset);
    //def("readPrimitiveBlock",&readPrimitiveBlock);
    
    init_primitve_block();
    init_geometry();
}
    


#ifndef READGEOMETRY
#define READGEOMETRY

#include "simplepbf.hpp"





struct point {
    int64 ref,lon,lat;
    point() : ref(0), lon(0), lat(0) {}
    point(int64 rf, int64 ln, int64 lt) : ref(rf), lon(ln), lat(lt) {}
};

typedef std::vector<point> Ring;
typedef std::vector<Ring> Poly;


class pointgeometry : public geometry {
    
    public:
        pointgeometry(point pt) : geometry(1, bbox(pt.lon,pt.lat,pt.lon,pt.lat)), pt_(pt) {}
            
        point Point() { return pt_; }
        
        virtual ~pointgeometry() {}
    private:
        point pt_;
};

class linegeometry : public geometry {
    
    public:
        linegeometry(Ring ln, int64 zo, bbox bx) : geometry(2, bx), ln_(ln), zo_(zo) {}
        
        int NumPoints() { return ln_.size(); }    
        point Point(int i) { return ln_[i]; }
        int64 ZOrder() { return zo_; }
        
        virtual ~linegeometry() {}
        
    private:
        Ring ln_;
        int64 zo_;
};

class polygongeometry : public geometry {
    
    public:
        polygongeometry(Poly py, int64 zo, double ar, bbox bx) : geometry(3, bx ), py_(py), zo_(zo), ar_(ar) {}
        
        int NumRings() { return py_.size(); }    
        int NumPoints(int i) { return py_[i].size(); }    
        point Point(int i, int j) { return py_[i][j]; }
        int64 ZOrder() { return zo_; }
        double WayArea() { return ar_; }
        
        virtual ~polygongeometry() {}
        
    private:
        Poly py_;
        int64 zo_;
        double ar_;
};

class multigeometry : public geometry {
    
    public:
        multigeometry(std::vector<Poly> pys, int64 zo, double ar, bbox bx) : geometry(7, bx ), pys_(pys), zo_(zo), ar_(ar) {}
        
        int NumGeometries() { return pys_.size(); }
        int NumRings(int i) { return pys_[i].size(); }    
        int NumPoints(int i, int j) { return pys_[i][j].size(); }    
        point Point(int i, int j, int k) { return pys_[i][j][k]; }
        
        int64 ZOrder() { return zo_; }
        double WayArea() { return ar_; }
        
        virtual ~multigeometry() {}
        
    private:
        std::vector<Poly> pys_;
        int64 zo_;
        double ar_;
};

#endif

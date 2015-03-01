"""A more complex example which renders an infinite series of concentric
circles centred on a point.

The circles are represented by a Python iterator which will yield only the
circles which intersect the query's bounding box. The advantage of this
approach over a MemoryDatasource is that a) only those circles which intersect
the viewport are actually generated and b) only the memory for the largest
circle need be available since each circle is created on demand and destroyed
when finished with.
"""
import math
import mapnik
from shapely.geometry import *
import rendertiles, readpbf, readblock, qts

import json,urllib,urllib2,struct,time


get_props = lambda pp: dict(fix(k,v) for k,v in pp.items())
def fix(k,v):
    return str(k),v
    
    if v=='NULL':
        return str(k),None
    if k in ('way_pixels','way_area'):
        return str(k),float(v)
    if k in ('z_order','layernotnull','height','width','prio'):
        return str(k),int(v)
    
    return str(k),v
        

usePG=False

#usePG=True

def make_query(idx,table,query,aspbf=False):
    t=table.strip()
    
    resx,resy = query.resolution
    if '!pixel_width!' in t:
        t=t.replace('!pixel_width!',"%0.5f" % (1./resx))
    if '!pixel_height!' in t:
        t=t.replace('!pixel_height!',"%0.5f" % (1./resy))
    
    data={'query':t,'response':'geojson'}
    if aspbf in (True,1):
        data['response'] = 'mapnik'
    elif aspbf==2:
        data['response'] = 'pbf'
        
    data['minx'] = query.bbox.minx
    data['miny'] = query.bbox.miny
    data['maxx'] = query.bbox.maxx
    data['maxy'] = query.bbox.maxy
    data['idx'] = idx
    dataenc=urllib.urlencode(data)
    req = urllib2.Request('http://localhost:17831/query',dataenc)
    
    return urllib2.urlopen(req)




def get_response_json(idx,table,query):
    t0=time.time()
    rs = make_query(idx,table,query).read()
    t1=time.time()
    result = json.loads(rs)
    if not result or 'features' not in result:
        print "no response??",repr(result)
        return  tuple(),tuple()
    if not result['features']:
        #raise Exception("no feats")
        #print "no feats for ", idx, repr(table)[:100]
        return tuple(),tuple()
    ff = result['features']
    keys = tuple(map(str,ff[0]['properties']))
    #print keys,len(ff)
    feats=[]
    for f in ff:
        try:
            feats.append((asShape(f['geometry']).wkb,get_props(f['properties'])))
        except ValueError as ex:
            print ex
            print f
    
    t2=time.time()
    print "json %-8d bytes, %-6d feats; %-3s fetch: %6.4fs; read: %6.4fs" % (idx,len(rs),len(feats),t1-t0,t2-t1)
    
    return mapnik.PythonDatasource.wkb_features(keys=keys,features=feats)
    #return keys,tuple(feats)

class mpfs:
    def __init__(self, data, rp):
        self.data=data
        self.rp = rp
    def __call__(self):
        return readblock.makeMapnikPbfFeatureset(self.data, self.rp)

def get_response_mapnik(idx,table,query):
    t0=time.time()
    data=make_query(idx,table,query,True).read()
    t1=time.time()
    #keys,feats = readpbf.readMapnikPbf(data,True)
    feats = readblock.makeMapnikPbfFeatureset(data,True)
    
    t2=time.time()
    #print "pbf %-3s %-8d bytes, %-6d feats; fetch: %6.4fs; read: %6.4fs" % (idx,len(data),len(feats),t1-t0,t2-t1)
    #print feats[0]
    #return mapnik.PythonDatasource.wkb_features(keys=keys,features=feats)
    return feats
    
#get_response = get_response_mapnik
#get_response = get_response_json

def get_response(idx,table,query):
    return mpfs(make_query(idx,table,query,True).read(), True)



def getGeojsonTile(idx,query,x,y,z):
    data = {'response':'geojson','idx':0}
    data['query'] = query
    data['idx'] = idx
    
    data['tilex']=x
    data['tiley']=y
    data['tilez']=z
    data['latlon']=True
    
    dataenc=urllib.urlencode(data)
    req = urllib2.Request('http://localhost:17831/query',dataenc)
    
    return urllib2.urlopen(req).read()
    

class PostDS(mapnik.PythonDatasource):
    def __init__(self, **kwargs):
        super(PostDS, self).__init__()
            #envelope=mapnik.Box2d(-20037508,-20037508,20037508,20037508),
            #data_type= mapnik.DataType.Vector)
        self.table = kwargs["table"]
        self.idx = kwargs['idx']
        
        
        self.envelope = mapnik.Box2d(-20037508,-20037508,20037508,20037508)
        self.data_type = mapnik.DataType.Vector
        self.ds=None
        
        self.last,self.lastbx = None,None
        #self.ds=mapnik.Datasource(type='postgis',table=self.table,dbname='gis')#,asynchronous_request=True,max_async_connection=4)

    def features(self, query):
        try:
            if self.ds:
                return self.ds.features(query).features
            if query.bbox == self.lastbx:
                return self.last()
            rr = get_response(self.idx,self.table,query)
            self.last = rr
            self.lastbx = query.bbox
            #print self.idx, len(rr.data), [len(f.to_geojson()) for f in rr()]
            return rr()
            
            
            
        except Exception as ex:
            print ex
            return mapnik.PythonDatasource.wkb_features(keys=tuple(),features=tuple())


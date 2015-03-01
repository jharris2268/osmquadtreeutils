import psycopg2
import shapely.wkb,shapely.geometry
import mapnik, compgeom as cg, PIL.Image
import json,gzip

def prep():
    return psycopg2.connect('dbname=gis'), cg.make_mapnik()

def handle_val(key,val):
    if key=='way':
        return shapely.wkb.loads(val.decode('hex'))
    return val

mkrow = lambda r, curs: dict((d.name, handle_val(d.name,v)) for d,v in zip(curs.description,r) if v!=None)

def getAll(conn, pbx):
    curs=conn.cursor()
    ans={}
    
    for tab in 'point line polygon roads'.split():
        ts="select * from planet_osm_"+tab+" where way && setsrid('BOX3D(%s %s, %s %s)'::box3d,4326)"
        curs.execute(ts,pbx)
        
        ans[tab] = [mkrow(r, curs) for r in curs]
    
    return ans

def sameObj(a,b):
    if a['osm_id']!=b['osm_id']:
        return False
    if 'Polygon' in a['way'].type:
        if a['way'].difference(b['way']).area / a['way'].area > 0.0001:
            return False
        if b['way'].difference(a['way']).area / b['way'].area > 0.0001:
            return False
    elif not a['way'].almost_equals(b['way']):
        return False
    
    kk = set(a)
    kk.update(b)
    for s in 'way osm_id parent_highway quadtree z_order way_area'.split():
        if s in kk:
            kk.remove(s)
    
    for k in kk:
        if not k in a or not k in b or a[k]!=b[k]:
            return False
    return True

def sortObjs(A):
    for v in A.values():
        v.sort(key=lambda x: x['osm_id'])
    

def getGeoJson(fn):
    res=dict(zip("point line polygon".split(),([],[],[])))
    for vl in json.load(gzip.open(fn) if fn.endswith('gz') else open(fn))['features']:
        qt=vl['properties']['quadtree']
        for o in vl['features']:
            no = o['properties'].copy()
            no['osm_id'] = o['id'] & 0xffffffffff
            if (o['id']>>59) == 2:
                no['osm_id']*=-1
            no['quadtree'] = qt
            t=o['geometry']['type']
            if t=='Point':
                no['way'] = shapely.geometry.Point(o['geometry']['coordinates'])
                res['point'].append(no)
            elif t=='Linestring':
                
                no['way'] = shapely.geometry.LineString(o['geometry']['coordinates'])
                res['line'].append(no)
            elif t=='Polygon':
                r0 = o['geometry']['coordinates'][0]
                if len(o['geometry']['coordinates'])==1:
                    no['way'] = shapely.geometry.Polygon(r0)
                else:
                    no['way'] = shapely.geometry.Polygon(r0,o['geometry']['coordinates'][1:])
                res['polygon'].append(no)
    return res
                    
            #o['properties']['qt'] = f

def rendermap(mp, **kwargs):
    if 'box' in kwargs:
        bx=kwargs['box']
        if type(bx) != mapnik.Box2d:
            bx=mapnik.Box2d(*bx)
        mp.zoom_to_box(bx)
    elif 'tl' in kwargs and 'sc' in kwargs:
        pp=mapnik.Projection(mp.srs)
        tl=pp.forward(mapnik.Coord(*kwargs['tl']))
        sc=kwargs['sc']
        bx=mapnik.Box2d(tl.x,tl.y-mp.height*sc,tl.x+mp.width*sc,tl.y)
        mp.zoom_to_box(bx)
    
    im=mapnik.Image(mp.width, mp.height)
    mapnik.render(mp,im)
    return PIL.Image.fromstring('RGBA',(mp.width, mp.height),im.tostring())
    
def nextOrNone(itr):
    try:
        return itr.next()
    except:
        return None
    
def combine(A,B,cmp):
    Ai=iter(A)
    Bi=iter(B)
    
    a=nextOrNone(Ai)
    b=nextOrNone(Bi)
    
    while a or b:
        if not b:
            yield a,None
            a=nextOrNone(Ai)
        elif not a:
            yield None,b
            b=nextOrNone(Bi)
        else:
            c = cmp(a,b)
            if c<0:
                yield a,None
                a=nextOrNone(Ai)
            elif c>0:
                yield None,b
                b=nextOrNone(Bi)
            else:
                yield a,b
                a=nextOrNone(Ai)
                b=nextOrNone(Bi)
    
    
def combineObjs(A,B):
    return list(combine(A,B,lambda x,y: cmp(x['osm_id'],y['osm_id'])))

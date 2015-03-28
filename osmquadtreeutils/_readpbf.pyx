import struct,cStringIO as StringIO,zlib
from collections import namedtuple
import time
import json,gzip,base64,math,os
import mapnik
#import hello

PrimitiveBlock = namedtuple("PrimitiveBlock","Idx Quadtree StartDate EndDate Objects Tags".split())
Node = namedtuple("Node","Id Info Tags Lon Lat Quadtree ChangeType".split())
Way = namedtuple("Way","Id Info Tags Nodes Quadtree ChangeType".split())
Relation = namedtuple("Relation","Id Info Tags Members Quadtree ChangeType".split())
Geometry = namedtuple("Geometry","Id Info Tags Geometry Quadtree ChangeType Bbox".split())
Tag = namedtuple("Tag","Key Value".split())
Member=namedtuple("Member","Type Ref Role".split())
Info=namedtuple("Info", "Version Timestamp Changeset Uid User Visible".split())

Point = namedtuple("Point", "Ref Lon Lat".split())
Linestring = namedtuple("Linestring", "Points ZOrder")
Polygon = namedtuple("Polygon", "Rings ZOrder Area")
MultiGeometry = namedtuple("MultiGeometry", "Geoms ZOrder Area")
BBox = namedtuple("BBox", "Minx Miny Maxx Maxy")

def quadTreeString(qt):
    if qt==None: return "NONE"
    if qt<0: return "NONE"
    return ''.join('ABCD'[(qt>>(61-2*i))&3] for i in range(qt&31))
    
boxString = lambda bx: "[%s]" % ", ".join("% 10.5f" %f for f in bx)
intersects=lambda a,b: not (a[0]>b[2] or a[1]>b[3] or a[2]<b[0] or a[3]<b[1])

def countObjs(bl):
    n,w,r,g = 0,0,0,0
    for o in bl.Objects:
        if type(o)==Node: n+=1
        if type(o)==Way: w+=1
        if type(o)==Relation: r+=1
        if type(o)==Geometry: g+=1
    return n,w,r,g

def countObjsGeom(bl):
    pt,ln,pl,mp,ot = 0,0,0,0,0
    for o in bl.Objects:
        if type(o)==Geometry:
            if type(o.Geometry)==Point: pt+=1
            elif type(o.Geometry)==Linestring: ln+=1
            elif type(o.Geometry)==Polygon: pl+=1
            elif type(o.Geometry)==MultiGeometry: mp+=1
            else: ot+=1
        else:
            ot+=1
    return pt,ln,pl,mp,ot

blockStr = lambda bl: "[%-6d] %-18s: %-50s %s" % (bl.Idx,quadTreeString(bl.Quadtree),boxString(quadTreeBounds(bl.Quadtree,0)),"%-6d %-5d %-4d %-5d" % countObjs(bl))
blockStrGeom = lambda bl: "[%-6d] %-18s: %-50s %s" % (bl.Idx,quadTreeString(bl.Quadtree),boxString(boundsBounds([o.Bbox for o in bl.Objects if type(o)==Geometry and o.Bbox])),"%-5d %-4d %-4d %-3d %d" % countObjsGeom(bl))

blockStrH = "[idx   ] %-18s: %-50s node   way   rel  geom" % ("quadtree", "box") 
blockStrHG = "[idx   ] %-18s: %-50s point line poly mp  other" % ("quadtree", "box")


def stringToQuadTree(s):
    q=len(s)
    for i,si in enumerate(s):
        q |= ((ord(si)-ord('A')) << (61-2*i))
    return q

def xyzToQuadTree(x,y,z):
    res = z
    for i in range(z):
        res |= ((x>>(z-1-i))&1) << 61-(2*i)
        res |= ((y>>(z-1-i))&1) << 62-(2*i)
    return res

def quadTreeBounds(qt, buf):
    
    a,b,c,d = -180.,-90.,180.,90.
    if qt==None:
        return a,b,c,d
    s=180.0
    for i in range(qt&31):
        x=(qt>>(61-2*i))&3
        if x&1: a+=s
        else: c-=s
        
        s/=2.0
        if x&2: d-=s
        else: b+=s
    
    a -= s*buf*2
    c += s*buf*2
    b -= s*buf
    d += s*buf
    return (a,unMerc(b),c,unMerc(d))

unMerc = lambda d: (math.atan(math.exp(d*math.pi/90.0))*4/math.pi - 1.0) * 90.0


def iterFileBlocks(inf):
    p=inf.tell()
    bt,bd=readFileBlock(inf)
    while bt:
        yield p,bt,bd
        p=inf.tell()
        bt,bd=readFileBlock(inf)

def readFileHeader(ind):
    blocktype=""
    blocklen=0
    
    for a,b,c in iterPbfTags(ind):
        if a==1:
            blocktype=c
        elif a==3:
            blocklen =b
    #print blocktype,blocklen
    return blocktype,blocklen
    
def readBlockData(ind):
    raw,rawlen,comp = "",0,""
    for a,b,c in iterPbfTags(ind):
        if a==1:
            raw=c
        elif a==2:
            rawlen=b
        elif a==3:
            comp=c
    
    if raw:
        return raw
    #print len(raw),rawlen,len(comp)
    return zlib.decompress(comp)

def RU(bs):
    x,s,i=0,0,0
   
    while True:
        nc = bs.read(1)
        if len(nc)!=1:
            return None
        b=ord(nc)
        i+=1
        if b < 0x80:
            if (i>12) or ( (i==12) and (b>1)):
                return x
            return (x | (b<<s))
        x += ( (b&0x7f) << s)
        s += 7

def RV(bs):
    return unzigzag(RU(bs))

def unzigzag(ux):
    if ux==None:
        return ux
    x = ux >> 1
    if ux&1 != 0:
        x = ~x
    return x
    

def iterPbfTags(ind):
    inp = StringIO.StringIO(ind)
    t = RU(inp)
    while t:
        typ,tag  = t&7, t>>3
        if typ==0:
            yield tag,RU(inp),None
        elif typ==2:
            yield tag,None,inp.read(RU(inp))
        else:
            raise Exception("unexpected typ %d" % typ)
        
        t=RU(inp)
        
def readPackIntDelta(ind):
    inp=StringIO.StringIO(ind)
    
    def _():
        c=0
        p=RV(inp)
        while p!=None:
            c+=p
            yield c
            p=RV(inp)
    return list(_())
    
def readPackInt(ind):
    inp=StringIO.StringIO(ind)
    
    def _():
        
        p=RU(inp)
        while p!=None:
            yield p
            p=RU(inp)
    return list(_())
    
def readFileBlock(inf):
    try:
        l,=struct.unpack('>L',inf.read(4))
        a,b = readFileHeader(inf.read(l))
        return a,readBlockData(inf.read(b))
    except:
        return None,None


def readQuadtree(ind):
    x,y,z=0,0,0
    for a,b,c in iterPbfTags(ind):
        if a==1: x=b
        if a==2: y=b
        if a==3: z=b
    return xyzToQuadTree(x,y,z)




def readStringTable(ind):
    st=[]
    for a,b,c in iterPbfTags(ind):
        if a==1:
            st.append(c)
    return st

def readDenseInfo(ind, st):
    vs,ts,cs,ui,us,vv = None,None,None,None,None,None
    for a,b,c in iterPbfTags(ind):
        if a==1: vs = readPackInt(c)
        elif a==2: ts = readPackIntDelta(c)
        elif a==3: cs = readPackIntDelta(c)
        elif a==4: ui = readPackIntDelta(c)
        elif a==5: us = readPackIntDelta(c)
        elif a==6: vv = readPackInt(c)
    
    if not vs:
        return None
    if not ts or not cs or not ui or not us:
        raise Exception("??",vs,ts,cs,ui,us)
    
    infs=[]
    for ii,(v,c,t,i,s) in enumerate(zip(vs,ts,cs,ui,us)):
        infs.append(Info(v,c,t,i,st[s],vv[ii] if vv else True))
    return infs
    
    

def readNode(c,st,ct):
    id_,inf,tags,qt,rem = readCommon(c,st)
    ln,lt=None,None
    for a,b,c in rem:
        if a==8: lt = unzigzag(b)
        if a==9: ln = unzigzag(b)
    return Node(id_,inf,tags,ln,lt,qt,ct)

def readWay(c,st,ct):
    id_,inf,tags,qt,rem = readCommon(c,st)
    wn=None
    for a,b,c in rem:
        if a==8: wn = readPackIntDelta(c)
        
    return Way(id_,inf,tags,wn,qt,ct)


def readRelation(c,st,ct):
    id_,inf,tags,qt,rem = readCommon(c,st)
    tt,rr,rl=None,None,None
    for a,b,c in rem:
        if a==8: rl = [st[i] for i in readPackInt(c)]
        if a==9: rr = readPackIntDelta(c)
        if a==10: tt= readPackInt(c)
    mems=None
    if rr:
        if not rl:
            rl=[None for i in rr]
        
        mems=[Member(t,r,l) for t,r,l in zip(tt,rr,rl)]
    
        
    return Relation(id_,inf,tags,mems,qt,ct)

def readPoint(ind):
    r,ln,lt = None,None,None
    for a,b,c in iterPbfTags(ind):
        if a==1: r=unzigzag(b)
        if a==2: ln=unzigzag(b)
        if a==3: lt=unzigzag(b)
    return Point(r,ln,lt)

def readLine(ind):
    r,ln,lt = None,None,None
    for a,b,c in iterPbfTags(ind):
        if a==1: r=readPackIntDelta(c)
        if a==2: ln=readPackIntDelta(c)
        if a==3: lt=readPackIntDelta(c)
    return [Point(x,y,z) for x,y,z in zip(r,ln,lt)]

def readPolygon(ind):
    pp = []
    for a,b,c in iterPbfTags(ind):
        if a==1: pp.append(readLine(c))
    return pp

def readGeoBbox(ind):
    mx,my,sx,sy = 0,0,0,0
    for a,b,c in iterPbfTags(ind):
        if a==1: mx = unzigzag(b)
        if a==2: my = unzigzag(b)
        if a==5: sx = unzigzag(b)
        if a==6: sy = unzigzag(b)
    return BBox(mx,my,mx+sx,my+sy)



def getGeomBbox(ind):
    gt,bx=None,None
    
    for a,b,c in iterPbfTags(ind):
        if a==10:
            gt=b
        elif a==13:
            p=readPoint(c)
            bx=BBox(p.Lon,p.Lat,p.Lon,p.Lat)
        elif a==16: bx=readGeoBbox(c)
        
        if gt and bx:
            return gt,bx
    if not bx:
        bx=geometryBoxInt(readGeom(list(iterPbfTags(ind)),None)[0])
    return gt,bx

def readGeometry(c,st,ct,filt=None):
    id_,inf,tags,qt,rem = readCommon(c,st)
    geo,bx = readGeom(rem,filt)
    if geo==None:
        return None
    return Geometry(id_,inf,tags,geo,qt,ct,bx)
    
   
def readGeom(rem,filt):
    gt,zo,ar,pts,lns,pls = None,None,None,[],[],[]
    if filt:
        for a,b,c in rem:
            if a==16:
                bx=boxToFloat(readGeoBbox(c))
                if not intersects(filt,bx):
                    return None,None
    
    for a,b,c in rem:
        if a==10: gt=b
        elif a==11: zo=unzigzag(b)
        elif a==12: ar=struct.unpack('>d',struct.pack('>Q',b))[0]
        elif a==13: pts.append(readPoint(c))
        elif a==14: lns.append(readLine(c))
        elif a==15: pls.append(readPolygon(c))
    
    geo=None
    if gt==1 and len(pts)==1:
        geo = pts[0]
    elif gt==2 and len(lns)==1:
        geo = Linestring(lns[0], zo)
    elif gt==3 and len(pls)==1:
        geo = Polygon(pls[0], zo, ar)
    elif gt==7:
        
        gg=[]
        gg += pts
        for ln in lns:
            gg.append(Linestring(ln,zo))
        for pl in pls:
            gg.append(Polygon(pl,zo,0))
        geo = MultiGeometry(gg, zo, ar)
    else:
        print "??? gt=%d, %d pts, %d lns, %d polys" % (gt,len(pts),len(lns),len(pls))
    
    bx=geometryBox(geo)
    if filt:
        
        if intersects(bx,filt):
            return geo,bx
        return None,None
    return geo,bx


def pointsBounds(pp):
    a,b,c,d = 1800000000,1800000000,-1800000000,-1800000000
    for p in pp:
        if p.Lon < a: a=p.Lon
        if p.Lat < b: b=p.Lat
        if p.Lon > c: c=p.Lon
        if p.Lat > d: d=p.Lat
    return (a,b,c,d)
    
def boundsBounds(bnds,ii=False):
    a = 180.0*(10000000 if ii else 1)
    b,c,d = a,-a,-a
    for bb in bnds:
        if bb[0]<a: a=bb[0]
        if bb[1]<b: b=bb[1]
        if bb[2]>c: c=bb[2]
        if bb[3]>d: d=bb[3]
    return a,b,c,d
def geometryBoxInt(geom):
    if type(geom) == Point:
        return geom.Lon, geom.Lat, geom.Lon, geom.Lat
    elif type(geom) == Linestring:
        return pointsBounds(geom.Points)
    elif type(geom) == Polygon:
        return pointsBounds(geom.Rings[0])
    elif type(geom) == MultiGeometry:
        return boundsBounds(map(geometryBoxInt,geom.Geoms),True)
    raise Exception("WTF:"+type(geom))

def geometryBox(geom):
    return boxToFloat(geometryBoxInt(geom))

def boxToFloat(inbox):
    a,b,c,d=inbox
    return a*0.0000001,b*0.0000001,c*0.0000001,d*0.0000001

def readCommon(ind, st):
    id_,inf,kk,vv,qt,rem = None,None,None,None,None,[]
    for a,b,c in iterPbfTags(ind):
        
        if a==1: id_ = b
        elif a==2: kk = [st[i] for i in readPackInt(c)]
        elif a==3: vv = [st[i] for i in readPackInt(c)]
        elif a==4: inf = readInfo(c,st)
        elif a==20: qt=unzigzag(b)
        else: rem.append((a,b,c))
    
    tags=[]
    if kk and vv:
        tags=[Tag(k,v) for k,v in zip(kk,vv)]
    return id_,inf,tags,qt,rem

def readInfo(ind, st):
    vs,ts,cs,ui,us,vv = None,None,None,None,None,True
    for a,b,c in iterPbfTags(ind):
        if a==1: vs = b
        elif a==2: ts = b
        elif a==3: cs = b
        elif a==4: ui = b
        elif a==5: us = st[b]
        elif a==6: vv = (b!=0)
    return Info(vs,ts,cs,ui,us,vv if vs else False)
        
    


def readDense(ind, st,ct):
    ids,inf,kv,ln,lt,qt = None,None,None,None,None,None
    for a,b,c in iterPbfTags(ind):
        if a==1: ids = readPackIntDelta(c)
        if a==5: inf = readDenseInfo(c,st)
        if a==8: lt = readPackIntDelta(c)
        if a==9: ln = readPackIntDelta(c)
        if a==10: kv = readPackInt(c)
        if a==20: qt = readPackIntDelta(c)
    
    tags=[]
    if kv:
        kvi = 0
        while kvi < len(kv):
            tt=[]
            while kv[kvi] != 0:
                tt.append(Tag(st[kv[kvi]],st[kv[kvi+1]]))
                kvi+=2
            kvi+=1
            tags.append(tt)
        
    objs=[]
    for i,id_ in enumerate(ids):
        objs.append(Node(\
            id_,
            inf[i] if inf else None,
            tags[i] if tags else None,
            ln[i],
            lt[i],
            qt[i] if qt else None,
            ct
        ))
    
    return objs

        
        
    

def readPrimitiveGroup(ind, st, isChange,filt=None):
    objs=[]
    ct=0
    
    oos=set()
    if isChange:
        for a,b,c in iterPbfTags(ind):
            if a==10:
                ct=b
                
    
    for a,b,c in iterPbfTags(ind):
        if a==1: objs.append(readNode(c,st,ct))
        elif a==2: objs += readDense(c,st,ct)
        elif a==3: objs.append(readWay(c,st,ct))
        elif a==4: objs.append(readRelation(c,st,ct))
        elif a==20:
            g=readGeometry(c,st,ct,filt)
            if g:
                objs.append(g)
        
        elif a==10 and isChange:
            #ct=b
            pass
        else: oos.add(a)
    
    if not objs and oos:
        print oos
    return objs
    
def readPrimitiveBlock(idx, ind, isChange, filt=None):
    st=None
    qt,startdate,enddate=None,None,None
    kk,vv=None,None
    pgs = []
    for a,b,c in iterPbfTags(ind):
        if a==1:
            st=readStringTable(c)
        elif a==2:
            pgs.append(c)
        elif a==31:
            qt=readQuadtree(c)
        elif a==33:
            startdate=b
        elif a==34:
            enddate=b
        elif a==35:
            kk=readPackInt(c)
        elif a==36:
            vv=readPackInt(c)
    objs=[]
    for pg in pgs:
        objs += readPrimitiveGroup(pg, st, isChange,filt)
    
    tags=None
    if kk and vv and st:
        tags=[Tag(st[k],st[v]) for k,v in zip(kk,vv)]
    return PrimitiveBlock(idx,qt,startdate,enddate,objs,tags)

def readPrimBlockIdAlloc(idx,ind):
    
    qt=None
    
    pgs = []
    for a,b,c in iterPbfTags(ind):
        if a==2:
            pgs.append(c)
        elif a==31:
            qt=readQuadtree(c)
        
    
    objs=[]
    for pg in pgs:
        objs += readPGIds(pg)
    return (idx,qt,objs)


def readPrimBlockIdQt(idx,ind):
    
    qt=None
    
    pgs = []
    for a,b,c in iterPbfTags(ind):
        if a==2:
            pgs.append(c)
        elif a==31:
            qt=readQuadtree(c)
        
    
    objs=[]
    for pg in pgs:
        objs += readPGIdsQts(pg)
    return (idx,qt,objs)

def getId(ind):
    
    for a,b,c in iterPbfTags(ind):
        if a==1: return b
        
    return None

def getIdQt(ind):
    ii,qt=None,None
    for a,b,c in iterPbfTags(ind):
        if a==1: ii=b
        elif a==20: qt=unzigzag(b)
        if ii!=None and qt!=None:
            return ii,qt
        
    return ii,qt

def getDenseIds(ind):
    for a,b,c in iterPbfTags(ind):
        if a==1: return [(0,i) for i in  readPackIntDelta(c)]
    return []

def getDenseIdsQts(ind):
    ii,qt=None,None
    for a,b,c in iterPbfTags(ind):
        if a==1: ii = readPackIntDelta(c)
        elif a==20: qt=readPackIntDelta(c)
        
        if ii and qt:
            return [(0,i,q) for i,q in zip(ii,qt)]
    if ii:
        return [(0,i,None) for i in ii]
    return []

def readPGIds(ind):
    oos=[]
    objs=[]
    for a,b,c in iterPbfTags(ind):
        if a==1: objs.append((0,getId(c)))
        elif a==2: objs += getDenseIds(c)
        elif a==3: objs.append((1,getId(c)))
        elif a==4: objs.append((2,getId(c)))
        elif a==20: objs.append((3,getId(c)))
        
        else: oos.add(a)
    
    if not objs:
        print oos
    return objs

def readPGIdsQts(ind):
    oos=[]
    objs=[]
    for a,b,c in iterPbfTags(ind):
        if a==1: objs.append((0,)+getIdQt(c))
        elif a==2: objs += getDenseIdsQts(c)
        elif a==3: objs.append((1,)+getIdQt(c))
        elif a==4: objs.append((2,)+getIdQt(c))
        elif a==20: objs.append((3,)+getIdQt(c)+getGeomBbox(c))
        
        else: oos.add(a)
    
    if not objs:
        print oos
    return objs

def readBbox(ind):
    mx,my,Mx,My=None,None,None,None
    for a,b,c in iterPbfTags(ind):
        if a==1: mx=unzigzag(b)/100
        if a==2: my=unzigzag(b)/100
        if a==3: Mx=unzigzag(b)/100
        if a==4: My=unzigzag(b)/100
    return [mx,my,Mx,My]

def readIdxItem(ind):
    qt,ty,blen = None,None,None
    for a,b,c in iterPbfTags(ind):
        if a==1: qt=readQuadtree(c)
        if a==2: isc=b!=0
        if a==3: blen = unzigzag(b)
    return [qt,isc,blen]
    


def readHeaderBlock(ind):
    bbox=None
    tags=[]
    idx=[]    
    
    for a,b,c in iterPbfTags(ind):
        if a==1:
            bbox=readBbox(c)
        elif a==4:
            tags.append(("required",c))
        elif a==5:
            tags.append(("optional",c))
        elif a==16:
            tags.append(("writingprogram",c))
        elif a==17:
            tags.append(("source",c))
        elif a==22:
            idx.append(readIdxItem(c))
    return bbox,tags,idx

def getHeaderBlock(fn):
    f=open(fn)
    a,b=readFileBlock(f)
    if a=='OSMHeader':
        x,y,z= readHeaderBlock(b)
        p=f.tell()
        for zi in z:
            zi.append(p)
            p+=zi[2]
        return x,y,z
    return None

def readBlockAt(fn, idxitem,idx=0,filt=None):
    f=open(fn)
    f.seek(idxitem[3])
    a,b=readFileBlock(f)
    return readPrimitiveBlock(idx,b,idxitem[1],filt)

def timeOp(op,*args,**kwargs):
    st=time.time()
    ans=op(*args,**kwargs)
    return time.time()-st, ans


def iterBlocks(fn,isc=False, rpb=readPrimitiveBlock):
    
    for i,(a,b,c) in enumerate(iterFileBlocks(open(fn))):
        if b in ('OSMData','OSMChange'):
            iscc=isc or b=='OSMChange'
            yield a, rpb(i,c,iscc)
        

def iterObjs(fn):
    for a,b in iterBlocks(fn):
        for o in b.Objects:
            yield o    

def readPackedInfo(ind):
    if not ind: return None
    ins = StringIO.StringIO(ind)
    
    vs,ts,cs,ui = RV(ins),RV(ins),RV(ins),RV(ins)
    us = ins.read(RU(ins))
    return Info(vs,cs,ts,ui,us)

def readPackedTags(ind):
    ins = StringIO.StringIO(ind)
    tt=[]
    for i in range(RV(ins)):
        #print i,ins.tell(),len(ind)
        k = ins.read(RU(ins))
        v = ins.read(RU(ins))
        tt.append(Tag(k,v))
    return tt

def readPackedMembers(ind):
    ins = StringIO.StringIO(ind)
    mm=[]
    
    r=0
    for i in range(RV(ins)):
        t=RU(ins)
        r += RV(ins)
        rl = ins.read(RU(ins))
        mm.append(Member(t,r,rl))
    return mm

def readPackedWaynodes(ind):
    ins = StringIO.StringIO(ind)
    mm=[]
    
    r=0
    for i in range(RV(ins)):
        r += RV(ins)
        mm.append(r)
    return mm
    
def readPackedObject(ind):
    ins = StringIO.StringIO(ind)
    t,i = struct.unpack('>bq',ins.read(9))
    ot = t&3
    ct = t>>7
    
    qt = RV(ins)
    data = ins.read(RU(ins))
    info = readPackedInfo(ins.read(RU(ins)))
    tags = readPackedTags(ins.read(RU(ins)))
    if ot==0:
        ln,lt = struct.unpack('>ll',data)
        return Node(i,info,tags,ln,lt,qt,ct)
    elif ot==1:
        wn = readPackedWaynodes(data)
        return Way(i,info,tags,wn,qt,ct)
    elif ot==2:
        mm = readPackedMembers(data)
        return Relation(i,info,tags,mm,qt,ct)
    elif ot==3:
        return Geometry(i,info,tags,data,qt,ct)
    raise Exception("??")
    #return ot,i,info,tags,data,ct
    
    
    
def readPackedJson(fn):
    fl=open(fn)
    if fn.endswith('gz'):
        fl=gzip.open(fn)
    
    r=[]
    for k,v in json.load(fl).iteritems():
        if 'Left' in v and len(v['Left'])>1:
            raise Exception("multi left")
        if 'Right' in v and len(v['Right'])>1:
            raise Exception("multi right")
        x=None
        if 'Left' in v and v['Left']:
            x=readPackedObject(base64.b64decode(v['Left'][0]))
        y=None
        if 'Right' in v and v['Right']:
            y=readPackedObject(base64.b64decode(v['Right'][0]))
        
        r.append((int(k),x,y))
    return sorted(r)


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

def combineFirst(A,B):
    return combine(A,B,lambda x,y: cmp(x[0],y[0]))


def filterObjs(bl, box):
    oo = []
    for o in bl.Objects:
        if type(o)!=Geometry:
            raise Exception("Not a Geometry")
        bx=geometryBox(o.Geometry)
        if intersects(box,bx):
            oo.append(Geometry(o[0],o[1],o[2],o[3],o[4],o[5],bx))
    return PrimitiveBlock(bl[0],bl[1],bl[2],bl[3],oo)
            
        
        

def readBlocksFilter(fn,box=None,filtObjs=False,printConts=False,outFunc=None):
    
    
    if printConts:
        print (blockStrHG if filtObjs else blockStrH)
        
    def _():
        if box==None:
            for a,bl in iterBlocks(fn):
                if printConts:
                    print (blockStrGeom if filtObjs else blockStr)(bl)
                yield bl
        else :
            idx=getHeaderBlock(fn)
            if not idx:
                raise Exception("no header")
            if not idx[2]:
                raise Exception("no index")
            for i,t in enumerate(idx[2]):
                if intersects(box, quadTreeBounds(t[0],0.05)):
                    bl = readBlockAt(fn, t,idx=i,filt=(box if filtObjs else None))
                    
                    if bl.Objects:
                        if printConts:
                            print (blockStrGeom if filtObjs else blockStr)(bl)
                        yield bl
    if not outFunc:
        return _()
    return outFunc(_())
    

def splitObjs(inblcks,asMerc=True,useShapely=False):
    #outs = {'point':[],'line':[],'polygon':[]}
    pts,lns,pls=[],[],[]
    for bl in inblcks:
        for obj in bl.Objects:
            o = toDict(obj,asMerc,useShapely=False)
            if o['way'][9]=='1':
                pts.append(o)
            elif o['way'][9]=='2':
                lns.append(o)
            else:
                pls.append(o)
    
    outs={}
    for k,v in zip('point line polygon'.split(),(pts,lns,pls)):
        v.sort(key=lambda o: o['osm_id'])
        kk=set()
        for vi in v: kk.update(vi)
        outs[k] = (sorted(kk),v)
        
    return outs

def merc(y):
    return math.log(math.tan(math.pi*(1.0+y/90.0)/4.0)) * 90.0 / math.pi

earth_half_circum = 20037508.3428

def Mercator(ln, lt):
    return ln * earth_half_circum / 180.0, merc(lt) * earth_half_circum / 90.0

def drop_repeats(pp):
    lp = None
    for p in pp:
        if p==lp:
            continue
        yield p
        lp=p

def pairs(pp):
    lp=None
    for p in pp:
        if lp!=None:
            yield lp,p
        lp=p

def is_clockwise(pp):
    tt=0
    for (a,b),(c,d) in pairs(pp):
        tt += (c-a)*(d+b)
    return tt>0
        

def fix_ring(pp,direc):
    pp=list(drop_repeats(pp))
    if is_clockwise(pp)!=(direc>0):
        pp.reverse()
    return pp

def ptxy(pt,asMerc):
    ln,lt = pt.Lon*0.0000001,pt.Lat*0.0000001
    if asMerc:
        return Mercator(ln,lt)
    return ln,lt
    
def pointWkb(pt,asMerc):
    x,y=ptxy(pt,asMerc)
    return struct.pack('>dd',x,y)

def ptxyRing(ring,asMerc):
    return [ptxy(p,asMerc) for p in ring]

def ringWkb(rr,asMerc,checkring=0):
    pp=ptxyRing(rr,asMerc)
    if checkring!=0:
        pp = fix_ring(pp,checkring)
    
    ans=struct.pack('>L',len(pp))
    for x,y in pp:
        ans+=struct.pack('>dd',x,y)
    return ans



import shapely.geometry,shapely.ops
def toShapely(geom,asMerc):
    if type(geom) == Point:
        return shapely.geometry.Point(*ptxy(geom,asMerc))
    if type(geom) == Linestring:
        return shapely.geometry.LineString(ptxyRing(geom.Points,asMerc))
    if type(geom) == Polygon:
        pp = [ptxyRing(rr,asMerc) for rr in geom.Rings]
        if len(pp)==1:
            return shapely.geometry.Polygon(pp[0]).buffer(0)
        return shapely.geometry.Polygon(pp[0],pp[1:]).buffer(0)
    
    ggs = map(toShapely,geom.Geoms)
    return shapely.ops.cascaded_union(ggs)
    
def toWkb(geom,asMerc=False,useShapely=False):
    if useShapely:
        return toShapely(geom,asMerc).wkb
    if type(geom) == Point:
        return '\0\0\0\0\1'+pointWkb(geom,asMerc)
    if type(geom) == Linestring:
        return '\0\0\0\0\2'+ringWkb(geom.Points,asMerc)
    if type(geom) == Polygon:
        ans = '\0\0\0\0\3'
        ans += struct.pack('>L',len(geom.Rings))
        for i,rr in enumerate(geom.Rings):
            cc = False #1 if i==0 else -1
            ans += ringWkb(rr,asMerc,cc)
    
    if type(geom) == MultiGeometry:
        ty='\6'
        for g in geom:
            if type(g)!=Polygon:
                ty='\7'
        ans = '\0\0\0\0'+ty
        ans += struct.pack('>L',len(geom.Geoms))
        for rr in geom.Geoms:
            ans += toWkb(rr,asMerc,useShapely)
    
    return ans
    
def toDict(obj,asMerc,useShapely=False):
    ans={}
    if obj.Id>>59 == 0:
        ans['osm_id'] = obj.Id
    elif obj.Id>>59 == 1:
        ans['osm_id'] = obj.Id&0xffffffffff
    elif obj.Id>>59 == 2:
        ans['osm_id'] = (obj.Id&0xffffffffff)*-1
    
    for k,v in obj.Tags:
        ans[k] = v
    if hasattr(obj.Geometry,'ZOrder'):
        ans['z_order'] = obj.Geometry.ZOrder
    if hasattr(obj.Geometry,'Area'):
        ans['way_area'] = obj.Geometry.Area
    
    ans['way'] = toWkb(obj.Geometry,asMerc,useShapely)#.encode('hex')
    ans["qt"] = quadTreeString(obj.Quadtree)
    return ans
        

def readMapnikPbfObj(i, keys, feat, ctx=None):
    wkb = None
    tt,vv = [],[]
    for a,b,c in iterPbfTags(feat):
        if a==3: tt.append(b)
        if a==4: vv.append(c)
        if a==5: wkb=c

    
    
    props={}
    if ctx!=None:
        props=mapnik.Feature(ctx,i)
        props.add_geometries_from_wkb(wkb)
    
    for k,t,v in zip(keys,tt,vv):
        if t==0:
            props[k]=v
        elif t==1:
            props[k] = RV(StringIO.StringIO(v))
        elif t==2:
            i=RU(StringIO.StringIO(v))
            props[k] = struct.unpack('>d',struct.pack('>Q',i))[0]
        elif t==3:
            props[k]=None
        
    #print len(wkb),props
    if ctx!=None:
        return props
    
    
    return wkb,props
    
    

def readMapnikPbf(data,asff=True):
    keys,feats = [],[]
    ctx=None
    if asff:
        ctx=mapnik.Context()
    for a,b,c in iterPbfTags(data):
        if a==4:
            keys.append(c)
            if asff:
                ctx.push(c)
        elif a==5:
            feats.append(c)
    
    
    return tuple(keys),[readMapnikPbfObj(i,keys,f,ctx) for i,f in enumerate(feats)]
    
def objCmp(x,y):
    if type(x)==type(y):
        return cmp(x.Id,y.Id) 
    if type(x)==Node or type(y) == Relation:
        return -1
    return 1
    
def mergeList(ll):
    if not ll:
        return []
    if len(ll)==1:
        return ll[0]
    return mergeChange(ll[0], mergeList(ll[1:]))

def mergeChange(A,B):
    return [b if b else a for a,b in combine(A,B,objCmp)]

def asNormal(o):
    ty=type(o)
    aa=list(o)[:-1]+[0,]
    return ty(*aa)
    

def applyChange(A, B):
    return [asNormal(o) for o in mergeChange(A,B) if not o.ChangeType in (1,2)]
    
getFns = lambda rt: sorted([rt+f for f in os.listdir(rt) if f.endswith('.pbfc') or (f.endswith('.pbf') and not '-qts' in f)])
getIdxes = lambda rt: [(f, getHeaderBlock(f)[2]) for f in getFns(rt)]

def getLocationAll(db, ty, rf):
    ii = (ty<<59) | rf
    k = ii/32
    
    mm = list(db.RangeIter(struct.pack('>qH',k,0),struct.pack('>qH',k,65535)))
    if not mm:
        return []
    return [getloc(readPackIntDelta(b)[ii%32]) for a,b in mm]

def getloc(ss):
    if ss==0:
        return None
    ss-=1
    return ss>>32, ss%0xffffffff

def getLocation(db, ty, rf):
    mm = getLocationAll(db, ty, rf)
    if not mm:
        return None
    return mm[-1]
    
    

def getLocationTiles(db):
    mm=list(db.RangeIter('\xff'*8+'\0\0'))
    if not mm:
        return
    qq=None
    if mm[-1][0]=='\xff'*10:
        qq=readPackIntDelta(mm[-1][1])
        mm.pop()
    
    tt=[]
    for k,v in mm:
        a, = struct.unpack('>H',k[-2:])
        vs=StringIO.StringIO(v)
        b=RV(vs)
        c=vs.read(RU(vs))
        d=RU(vs)
        tt.append((a,b,c,d))
    
    return tt,qq
        
        

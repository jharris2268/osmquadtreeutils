import mapnik
import subprocess,PIL.Image,cStringIO as StringIO
import time,sys,os


ew = 20037508.3428
tz = 8

def make_mapnik(fn, tabpp = None, scale=None, srs=None, mp=None, avoidEdges=False, abspath=True):
    
        
    cc=[l for l in subprocess.check_output(['carto',fn]).split("\n") if not l.startswith('[millstone')]
    
    if scale!=None:
        for i,c in enumerate(cc):
            if 'ScaleDenominator' in c:
                sd=c.strip()[21:-22]
                nsd=str(int(sd)*scale)
                #print i,sd,"=>",nsd,
                c=c.replace(sd, nsd)
                #print c
                cc[i]=c
    
    
    bsp=''
    if abspath:
        a,b=os.path.split(fn)
        if a:
            bsp=a
            
        
            #for i,c in enumerate(cc):
            #    if 'file' in c:
            #        if 'file=' in c:
            #            cc[i] = c.replace('file="','file="'+a+'/')
            #        elif 'name="file"><![CDATA[' in c:
            #            cc[i] = c.replace('CDATA[','CDATA['+a+'/')
            
                        
    
    
    if avoidEdges:
        for i,c in enumerate(cc):
            if '<ShieldSymbolizer size' in c:
                cs = c.replace("ShieldSymbolizer size", "ShieldSymbolizer avoid-edges=\"true\" size")
                cc[i]=cs
    
    if tabpp != None:    
        cc=[l.replace("planet_osm",tabpp) for l in cc]
    
    
    
        
    
    #cc2=[c.replace("clip=\"false","clip=\"true") for c in cc]
    #cc3=[c.replace("file=\"symbols", "file=\""+root+"/symbols") for c in cc2]
    #cc4=[c.replace("CDATA[data", "CDATA["+root+"/data") for c in cc3]
    if mp==None:
        mp = mapnik.Map(256*tz,256*tz)        
        
    mapnik.load_map_from_string(mp,"\n".join(cc),False,bsp)
    
    if srs!=None:
        mp.srs=srs
    #mp.buffer_size=128
    
    return mp

def tilebound(z,x,y,tzp):
    zz = 1<<(z-1)
    ss = ew/zz * tzp
    xx = x / tzp
    yy = y / tzp
    bx = mapnik.Box2d(-ew + ss*xx, ew-ss*(yy+1), -ew+ss*(xx+1), ew-ss*yy)
    mm = "%d %d %d {%d %d %d %f} => %s" % (z,x,y,zz,xx,yy,ss,bx)
    return xx,yy,mm,bx

def render_im(mp,bx,width,height=None, scale_factor=1.0, buffer_size=256):
    if height==None:
        height=width
    mp.resize(width,height)
    mp.zoom_to_box(bx)
    mp.buffer_size = buffer_size
    im=mapnik.Image(mp.width,mp.height)
    mapnik.render(mp,im, scale_factor)
    return PIL.Image.frombytes('RGBA',(mp.width,mp.height),im.tostring())

def render_tile(mp,z,x,y):
    st=time.time()
    tzp = 1
    if z==13: tzp=2
    if z==14: tzp=4
    if z>=15: tzp=8
    #tzp = tz if z>10 else 1
    
    xx,yy,mm,bx=tilebound(z,x,y,tzp)
    print mm,
    sys.stdout.flush()
    pim = render_im(mp,bx,tzp*256)
    print "%-8.1fs" % (time.time()-st,)
    
    return iter_subtiles(pim,xx,yy,z,tzp)
    
def iter_subtiles(pim, xx,yy,z,tzp,ts=256):
    
    
    for i in xrange(tzp):
        for j in xrange(tzp):
            xp = xx*tzp+i
            yp = yy*tzp+j
            pp = pim.crop([i*ts,j*ts,(i+1)*ts,(j+1)*ts])
            #return pim.tostring('png')
            ss=StringIO.StringIO()
            pp.save(ss,format='PNG')
            yield (z,xp,yp),ss.getvalue()
            

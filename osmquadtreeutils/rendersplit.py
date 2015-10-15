import mapnik, csv, time, sys
import rendertiles as rt
import PIL.Image as pi

topil = lambda im: pi.fromstring('RGBA',(im.width(),im.height()),im.tostring())

def set_layers(mp, ss):
    for i in xrange(len(mp.layers)):
        mp.layers[i].active = (i in ss)

def load_groups(fn):
    ll = list(csv.reader(open(fn)))
    
    res = []
    hh = ll[0][2:]
    print hh, len(ll[1:])
    for l in ll[1:]:
        k = int(l[0])
        vv= [h for h,i in zip(hh,l[2:]) if i]
        if not vv:
            continue
        if len(vv)>1:
            raise Exception("row %d [%0.100s] in %d cols [%s]" % (k,l[1],len(vv),"; ".join(vv)))
        v = vv[0]
        if (not res) or v!=res[-1][0]:
            res.append([v,[k]])
        else:
            res[-1][1].append(k)
    print res
    return res

def render_im_split(mp, grps, bx, w, h=None,scale_factor=1.):
    if h==None:
        h=w
    mp.resize(w,h)    
    mp.zoom_to_box(bx)
    
    #w,h = mp.width,mp.height
    mc = mp.background
    
    mp.background = mapnik.Color(0,0,0,0)
    detector=mapnik.LabelCollisionDetector(mp)
    
    imgs = {}
    for a,b in grps:
        if not b:
            continue
        
        if b[0]==0:
            imgs[a] = pi.new('RGBA',(w,h), (mc.r,mc.g,mc.b,mc.a))
            if len(b)==1:
                continue
        
        im = mapnik.Image(w,h)
        
        set_layers(mp, b)
        mapnik.render_with_detector(mp, im, detector,scale_factor)
        pim = topil(im)
        
        if a not in imgs:
            imgs[a]=pim
        else:
            imgs[a] = pi.alpha_composite(imgs[a], pim)
        
    mp.background = mc
    
    return imgs
        
def render_tile_split(mp, grps, z, x, y, tzp=None,scale_factor=1):
    st=time.time()
    if tzp==None:
        tzp = 1
        if z==13: tzp=2
        if z==14: tzp=4
        if z>=15: tzp=8
        #tzp = tz if z>10 else 1
    
    xx,yy,mm,bx=rt.tilebound(z,x,y,tzp)
    print mm,
    sys.stdout.flush()
    #mp.resize(tzp*256, tzp*256)
    #mp.buffer_size = 256
    #mp.zoom_to_box(bx)
    pims = render_im_split(mp,grps,bx,tzp*256*scale_factor,tzp*256*scale_factor,scale_factor)
    print "%-8.1fs" % (time.time()-st,)
    
    for k,v in pims.iteritems():
        for x,y in rt.iter_subtiles(v,xx,yy,z,tzp):
            yield k, x, y
            
            
    
    
    
    
    

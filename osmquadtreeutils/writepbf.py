import zlib,struct
import cStringIO as StringIO
import readpbf as rp

def packUVarint(val):
    if val < 0:
        raise Exception("EXPECTED UNSIGNED")
    out = ""
    while val >= 0x80:
        out += chr( (val&127)|128 )
        val >>= 7
    out += chr(val)
    return out

def writeUVarint(file, val):
    file.write(packUVarint(val))

def packPbfTags(tags, file=None):
    retstr = False
    if file==None:
        retstr=True
        file = StringIO.StringIO()
    
    for a,b,c in tags:  
        t = a<<3
        if c!=None:
            t |= 2
        writeUVarint(file, t)
        if c==None:
            writeUVarint(file,b)
        else:
            writeUVarint(file,len(c))
            file.write(c)
    if retstr:
        file.seek(0)
        return file.read()
    return file

def makePbfFileBlock(blockType, blockData, compress=True):
    aa = []
    if compress:
        aa = [(2,len(blockData),None), (3, 0, zlib.compress(blockData))]
    else:
        aa = [(1,None,blockData)]
    
    aap = packPbfTags(aa)
    
    bb = [(1,0, blockType), (3,len(aap),None)]
    bbp = packPbfTags(bb)
    
    return struct.pack('>L',len(bbp))+bbp+aap
    

def writePbfFileBlock(file, blockType, blockData, compress=True):
    mm=makePbfFileBlock(blockType,blockData,compress)
    file.write(mm)
    return len(mm)
    
def writeBlocksIndex(file, blocks, blockType='OSMData', compress=True):
    idx=[]
    for key, data in blocks:
        
        idx.append((key, 0, writePbfFileBlock(file, blockType, data, compress)))
        
    return idx

def packQuadtree(q):
    x,y,z=rp.quadTreeXyz(q)
    return packPbfTags([(1,x,None),(2,y,None),(3,z,None)])

defOpts=[('required', 'OsmSchema-V0.6'), ('required', 'DenseNodes'), ('writingprogram', 'osmquadtree')]
def makeHeaderBlock(bbox=None, opts=None, idx=None):
    
    tags=[]
    if bbox!=None:
        bbt=[(i+1,rp.zigzag(s*100),None) for i,s in enumerate(bbox)]
        tags.append((1, 0, packPbfTags(bbt)))
    if opts==None:
        opts=defOpts
    for i,(k,v) in enumerate(opts):
        if k=='required': tags.append((4,i,v))
        elif k=='optional': tags.append((5,i,v))
        elif k=='writingprogram': tags.append((16,i,v))
        elif k=='source': tags.append((17,i,v))
    
    tags.sort()
    
    if idx!=None:
        for ii in idx:
            a,b,c=ii[:3]
            idp = [(1,0,packQuadtree(a)), (2,1 if b else 0,None), (3,rp.zigzag(c),None)]
            tags.append((22,0,packPbfTags(idp)))
    
    
    return packPbfTags(tags)
            


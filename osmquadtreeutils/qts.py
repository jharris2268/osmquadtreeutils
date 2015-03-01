import math

def stringToQuadTree(ss):
    res=len(ss)
    for i,c in enumerate(ss):
        l=0
        if c in 'BD': l|=1
        if c in 'CD': l|=2
        res |= (l  << (61-2*i))

    return res

def xyzToQuadTree(x,y,z):
    ans = 0
    scale = 1
    for i in xrange(z):
        ans += ((x>>i)&1|((y>>i)&1)<<1) * scale
        scale *= 4
    
    ans <<= (63 - (2 * z))
    ans |= z
    return ans

def merc(y):
    return math.log(math.tan(math.pi*(1.0+y/90.0)/4.0)) * 90.0 / math.pi

def calcQuadTree(mx, my, Mx, My, mxl, bf):
	if mx > Mx or my > My:
		return -1
	if Mx == mx:
		Mx += 0.0000001
	if My == my:
		My += 0.0000001
        
	mym = merc(my) / 90.0
	Mym = merc(My) / 90.0
	mxm = mx / 180.0
	Mxm = Mx / 180.0

	return makeQuadTree_(mxm, mym, Mxm, Mym, mxl, bf, 0)


def findQuad(mx, my, Mx, My, bf):
    if mx < (-1-bf) or my < (-1-bf) or Mx > (1+bf) or My > (1+bf):
        return -1
    if (Mx <= 0) and (my >= 0):
        return 0
    elif (mx >= 0) and (my >= 0):
        return 1
    elif (Mx <= 0) and (My <= 0):
        return 2
    elif (mx >= 0) and (My <= 0):
        return 3
    
    elif (Mx < bf and abs(Mx) < abs(mx)) and (my > -bf and abs(My) >= abs(my)):
        return 0
    elif (mx > -bf and abs(Mx) >= abs(mx)) and (my > -bf and abs(My) >= abs(my)):
        return 1
    elif (Mx < bf and abs(Mx) < abs(mx)) and (My < bf and abs(My) < abs(my)):
        return 2
    elif (mx > -bf and abs(Mx) >= abs(mx)) and (My < bf and abs(My) < abs(my)):
        return 3
        
	return -1


def makeQuadTree_(mx, my, Mx, My, mxl, bf, cl):
    if mxl == 0:
        return 0
    
    q = findQuad(mx, my, Mx, My, bf)
	
    if q == -1:
		return 0
	
    if q == 0 or q == 2:
        mx += 0.5
        Mx += 0.5
    
    else:
        mx -= 0.5
        Mx -= 0.5
    
    if q == 2 or q == 3:
        my += 0.5
        My += 0.5
    
    else:
        my -= 0.5
        My -= 0.5
	
    r = makeQuadTree_(2*mx, 2*my, 2*Mx, 2*My, mxl-1, bf, cl+1)
    #print q, (q << (61 - 2*cl)) + 1, r
    
    return (q << (61 - 2*cl)) + 1 + r


def quadTreeToXyz(qt):
    z = qt & 31
    x = 0
    y = 0
    for i in xrange(z):
        x <<= 1
        y <<= 1
        t = (qt >> (61-2*i)) & 3
        if (t & 1) == 1:
            x |= 1
        if (t & 2) == 2:
            y |= 1
    return x, y, z



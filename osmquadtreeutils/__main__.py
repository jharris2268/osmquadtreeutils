from bottle import route, run, static_file, request, post,response,template,hook
import bottle

bottle.TEMPLATE_PATH = ['/home/james/gopath/osmquadtreeutils/static']
import rendertiles, rendersplit
import sys,os

import urllib,json
import postds as ps

smalls=[]
def find_small(v):
    for i,s in enumerate(smalls):
        if v==s:
            return i
    i=len(smalls)
    smalls.append(v)
    return i


@route('/tile/<layer>/<zs>/<xs>/<ys>.png')
def tile(layer,zs,xs,ys):
    global mp
    global smalls
    response.content_type = "image/png"
    
    z,x,y = map(int,(zs,xs,ys))
    if not (z,x,y) in tiles[layer]:
        mpp = mp[layer]
        if type(mpp)==tuple:
            for ka,kb,v in rendersplit.render_tile_split(mpp[0],mpp[1],z,x,y):
                if len(v)<1000:
                    v = find_small(v)
                tiles['split_'+ka][kb] = v
        else:
            for k,v in rendertiles.render_tile(mpp,z,x,y):
                if len(v)<1000:
                    v = find_small(v)
                    
                tiles[layer][k]=v
    
    tt=tiles[layer][z,x,y]
    if type(tt)==int:
        return smalls[tt]
    return tt


@route('/tilespec.geojson')
def tilespec():
    response.content_type='application/json'
    return urllib.urlopen('http://localhost:17831/tiles').read()


roadsquery = """(
SELECT * FROM (
    (SELECT osm_id,quadtree,name,ref,way,feature,horse,foot,bicycle,tracktype,access,construction,service,oneway,link,layernotnull,prio FROM 
        (SELECT osm_id,quadtree,name,ref,way, ('highway_' || (CASE WHEN substr(highway, length(highway)-3, 4) = 'link' THEN substr(highway, 0, length(highway)-4) ELSE highway END)) AS feature, horse, foot, bicycle, tracktype, CASE WHEN access IN ('destination') THEN 'destination'::text WHEN access IN ('no', 'private') THEN 'no'::text ELSE NULL END AS access, construction, CASE WHEN service IN ('parking_aisle', 'drive-through', 'driveway') THEN 'INT-minor'::text ELSE 'INT-normal'::text END AS service, CASE WHEN oneway IN ('yes', '-1') THEN oneway ELSE NULL END AS oneway, CASE WHEN substr(highway, length(highway)-3, 4) = 'link' THEN 'yes'::text ELSE 'no'::text END AS link, makeinteger(layer) AS layernotnull FROM planet_osm_line ) 
            as a JOIN (VALUES ('highway_motorway','no',380), ('highway_trunk','no',370), ('highway_primary','no',360), ('highway_secondary','no',350), ('highway_tertiary','no',340), ('highway_residential','no',330), ('highway_unclassified','no',330), ('highway_road','no',330), ('highway_living_street','no',320), ('highway_pedestrian','no',310), ('highway_raceway','no',300), ('highway_motorway','yes',240), ('highway_trunk','yes',230), ('highway_primary','yes',220), ('highway_secondary','yes',210), ('highway_tertiary','yes',200), ('highway_service','no',150), ('highway_track','no',110), ('highway_path','no',100), ('highway_footway','no',100), ('highway_bridleway','no',100), ('highway_cycleway','no',100), ('highway_steps','no',100), ('highway_platform','no',90), ('highway_proposed','no',20), ('highway_construction','no',10) ) AS ordertable (feature, link, prio) 
        USING (feature,link) )
UNION ALL 
    (SELECT osm_id,quadtree,name,ref,way, feature,horse,foot,bicycle,tracktype,access,construction,service,oneway,link,layernotnull,prio FROM
        (SELECT osm_id,quadtree,name, ref, way, COALESCE(('railway_' ||(CASE WHEN railway = 'preserved' AND service IN ('spur', 'siding', 'yard') THEN 'INT-preserved-ssy'::text WHEN (railway = 'rail' AND service IN ('spur', 'siding', 'yard')) THEN 'INT-spur-siding-yard' ELSE railway end)), ('aeroway_' || aeroway)) AS feature, horse, foot, bicycle, tracktype, CASE WHEN access IN ('destination') THEN 'destination'::text WHEN access IN ('no', 'private') THEN 'no'::text ELSE NULL END AS access, construction, CASE WHEN service IN ('parking_aisle', 'drive-through', 'driveway') THEN 'INT-minor'::text ELSE 'INT-normal'::text END AS service, NULL::text AS oneway, 'no'::text AS link, makeinteger(layer) AS layernotnull FROM planet_osm_line )
            as c JOIN (VALUES ('railway_rail', 430), ('railway_spur', 430), ('railway_siding', 430),('railway_INT-spur-siding-yard',430), ('railway_subway', 420), ('railway_narrow_gauge', 420), ('railway_light_rail', 420), ('railway_preserved', 420), ('railway_funicular', 420), ('railway_monorail', 420), ('railway_miniature', 420), ('railway_turntable', 420), ('railway_tram', 410), ('railway_disused', 400), ('railway_construction', 400), ('aeroway_runway', 60), ('aeroway_taxiway', 50), ('railway_platform', 90) ) AS ordertable (feature, prio)
        USING (feature) )
) as features ORDER BY prio )
    AS roads_casing"""


@hook('after_request')
def enable_cors():
    response.headers['Access-Control-Allow-Origin'] = '*'

@route('/roads/<z>/<x>/<y>.json')
def getRoads(z,x,y):
    response.content_type='application/json'
    jj=json.loads(ps.getGeojsonTile(-1,roadsquery, x,y,z))
    jj.pop('properties')
    return jj

@route('/')
def fetchindex():
    global idxfn
    print request.query.items()
    lat,lon = '51.39','0.09'
    if 'lat' in request.query:
        lat=request.query['lat']
    if 'lon' in request.query:
        lon=request.query['lon']
    print "fetchindex",lat,lon
    response.content_type = "text/html"
    #return idx % (lat,lon)
    return template('index',ln=lon,lt=lat,hasOrig=True)
    #return static_file("index.html",root=staticloc)

#@route('/setloc')
#def setloc():
#    return {'x': 0.09, 'y': 51.395, 'z': 15}


@route('/images/<filename>')
def server_js(filename):
    global staticloc
    return static_file(filename, root=staticloc)

@route('/<filename>')
def server_js(filename):
    global staticloc
    return static_file(filename, root=staticloc)


if __name__ == "__main__":
    root='/home/james/map_data/openstreetmap-carto'
    fnalt='project-oqt.mml'
    
    grps=rendersplit.load_groups(os.path.join(root,fnalt[:-4]+'-tabs.csv'))
    
    mpa = rendertiles.make_mapnik(os.path.join(root,fnalt), avoidEdges=True)
    
    mp = {
        'orig': rendertiles.make_mapnik(os.path.join(root,'project.mml'),tabpp='se_eng_20150211'),
        'alt': mpa
    }
    
    
    for k in set(a for a,b in grps):
        mp['split_'+k] = (mpa, grps)
        #tiles['split_'+k] = {}
    
    tiles=dict((k, {}) for k in mp)
    print tiles
    staticloc='/home/james/gopath/osmquadtreeutils/static'
    #idx=open(staticloc+"/index.html").read()
    idxfn = staticloc+'index.html'
    run(host='0.0.0.0', port=8351, debug=True)



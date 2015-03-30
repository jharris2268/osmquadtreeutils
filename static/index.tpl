<!DOCTYPE html>
<html>
<head>
	<title>Leaflet Quick Start Guide Example</title>
	<meta charset="utf-8" />

	<meta name="viewport" content="width=device-width, initial-scale=1.0">

	<link rel="stylesheet" href="http://cdn.leafletjs.com/leaflet-0.7.3/leaflet.css" />

</head>
<body>
	<div id="map" style="width: 1200px; height: 800px"></div>
    <script src="https://code.jquery.com/jquery-1.11.2.min.js"></script>
    
	<script src="http://cdn.leafletjs.com/leaflet-0.7.3/leaflet.js"></script>
    <script src="TileLayer.GeoJSON.js"></script>
	<script>

		var osmUrl='http://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png';
        var osmAttrib='Map data © <a href="http://openstreetmap.org">OpenStreetMap</a> contributors';
        var osm = new L.TileLayer(osmUrl, {maxZoom: 18, attribution: osmAttrib});		

        %if hasOrig:
        
        var orig = L.tileLayer('tile/orig/{z}/{x}/{y}.png', {
			maxZoom: 18,
			attribution: 'local tiles',
			
		});
        
        %end
        
        var alt = L.tileLayer('tile/alt/{z}/{x}/{y}.png', {
            minZoom: 10,
			maxZoom: 18,
			attribution: 'local tiles',
			
		});
        
        
        var spt_other = {};
        
        %if hasSplit:
        var spt = L.tileLayer('tile/split_BASE/{z}/{x}/{y}.png', {
            minZoom: 10,
			maxZoom: 18,
			attribution: 'local tiles',
			
		});
        
        var kk = "LAND BUILD TRANS LABEL ADMIN ROADNAME".split(" ");
        for (var i in kk) {
            var k = kk[i];
            spt_other[k] = L.tileLayer('tile/split_'+k+'/{z}/{x}/{y}.png', {
                minZoom: 10, maxZoom: 18,attribution: 'local tiles'
            });
        }
        %end
        
        var map = L.map('map', {center: [{{lt}}, {{ln}}], zoom: 15, layers: [osm]});//.setView([51.505, -0.09], 13);
        
        
		
        var popup = L.popup();
        
        function rad(l) { return l*Math.PI/180.0; }
        function getTileURL(lat, lon, zoom) {
            var xtile = parseInt(Math.floor( (lon + 180) / 360 * (1<<zoom) ));
            var ytile = parseInt(Math.floor( (1 - Math.log(Math.tan(rad(lat)) + 1 / Math.cos(rad(lat))) / Math.PI) / 2 * (1<<zoom) ));
            return "" + zoom + "/" + xtile + "/" + ytile;
        }
		
        function onMapClick(e) {
			popup
				.setLatLng(e.latlng)
				.setContent("You clicked the map at " + e.latlng.toString() +" [tile: " + getTileURL(e.latlng.lat, e.latlng.lng, map.getZoom())+"]")
				.openOn(map);
		}
        
        
        
        
        
		map.on('click', onMapClick);
        
        var tilesOverlay = L.geoJson({},{
            style: function(f) { 
                if (f.properties && f.properties.z < 8) {
                    return {color: 'green', fillOpacity: 0.01};
                }
                return { fillOpacity: 0.1};
                
            },
            filter: function(f, l) {
                if (f.properties === undefined) { return false; }
                //if (f.properties.z < 8) { return false; }
                return true;
            },
            onEachFeature: function(f,l) {
                if (f.properties) {
                    var p = f.properties;
                    var tl = p.z+"/"+p.x+"/"+p.y;
                    var lp = L.popup();
                    lp.setContent("Tile "+tl+": "+p.features+"<br/>features, "+p.parents+" parent tiles<br>"+p.bbox)
                    
                    l.bindPopup(lp);
                }
            },
            });
        
        
        
        
        var roadStyle = {
        "clickable": true,
        "color": "#00D",
        "fillColor": "#00D",
        "weight": 4.0,
        "opacity": 0.3,
        "fillOpacity": 0.2
    };
    var hoverStyle = {
        "fillOpacity": 0.5
    };
        
        var roadsURL = 'roads/{z}/{x}/{y}.json';
        var roadsLayer = new L.TileLayer.GeoJSON(roadsURL, {
            clipTiles: true,
            unique: function (feature) {
                return feature.properties.osm_id; 
            },
            minZoom:14
        }, {
            style: roadStyle,
            onEachFeature: function (feature, layer) {
                if (feature.properties) {
                    var popupString = '<div class="popup">';
                    for (var k in feature.properties) {
                        var v = feature.properties[k];
                        popupString += k + ': ' + v + '<br />';
                    }
                    popupString += '</div>';
                    layer.bindPopup(popupString);
                }
                if (!(layer instanceof L.Point)) {
                    layer.on('mouseover', function () {
                        layer.setStyle(hoverStyle);
                    });
                    layer.on('mouseout', function () {
                        layer.setStyle(roadStyle);
                    });
                }
            }
        }
    );
    //map.addLayer(roadsLayer);
    
    var oo = spt_other;
    //oo["quadtree"] = tilesOverlay;
    oo["roads_overlay"] = roadsLayer;
    
    
    
    L.control.layers({"osm":osm,{{!"'orig':orig," if hasOrig else ""}}"alt": alt {{! ", 'split':spt" if hasSplit else ""}} }, oo).addTo(map);
    
    
        

	</script>
    <script>$(document).ready(function() {
        /*$.ajax({
            url: "setloc",
            type: "GET",
            dataType: "json",
            success: function( json ) {
                map.setView([json.y, json.x], json.z);
            },
            error: function(e) { alert(e); }
        });*/
        
        $.ajax({
            url: "tilespec.geojson",
            type: "GET",
            dataType: "json",
            success: function( json ) {
                tilesOverlay.addData(json)
            },
            error: function(e) { alert(e); }
        });
        
    })</script>
    
</body>


</html>

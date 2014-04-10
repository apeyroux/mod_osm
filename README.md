# mod_osm

Module Apache pour rendre des mbtiles OSM

```
<VirtualHost *:80>
	     ServerAdmin webmaster@localhost
	     ServerName osm.intra

	     #
	     # Configuration du module
	     #
	     LoadModule osm_module /usr/lib/apache2/modules/mod_osm.so
	     OsmEnabled true # enable du module (true|false)
	     OsmMbtiles0Path "/var/www/0.mbtiles" # path du mbtiles 0
	     OsmMbtiles1to9Path "/var/www/osmfr-z1-z9.mbtiles" # path z1 à z9
	     OsmMbtiles10to14Path "/var/www/osmfr-z10-z14.mbtiles" # path z10 à z14

	     # 
	     # balancer 
	     #
	     <Proxy balancer://pxy_osmfr_cluster>
	     	BalancerMember http://a.tile.openstreetmap.fr/osmfr 
    		BalancerMember http://b.tile.openstreetmap.fr/osmfr
    		BalancerMember http://c.tile.openstreetmap.fr/osmfr
	     </Proxy>	     

	     # les z10 à z19 passe par osmfr
	     # rgxg range -Z 10 19 
	     ProxyPassMatch ^/(1[0-9])/(\d+)/(\d+.png)$  balancer://pxy_osmfr_cluster/$1/$2/$3[1]

	     # tjrs utile suite ajout mbtiles ?
	     LoadModule cache_module modules/mod_cache.so
         <IfModule mod_disk_cache.c>
            CacheRoot /var/www/cache/apache/osm
            CacheEnable disk /
            CacheDirLevels 3
			CacheDirLength 2
	     </IfModule>

</VirtualHost>	
```

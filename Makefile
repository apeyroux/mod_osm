all:
	sudo apxs2 -i -a -l sqlite3 -c mod_osm.c 
	sudo /etc/init.d/apache2 restart

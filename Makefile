all:
	sudo apxs2 -i -a -lsqlite3 -c mod_osm.c
	sudo /etc/init.d/apache2 restart

#include "httpd.h" 
#include "http_config.h" 
#include "http_core.h" 
#include "http_log.h" 
#include "http_main.h" 
#include "http_protocol.h" 
#include "http_request.h" 
#include "util_script.h" 
#include "http_connection.h"

#include <sqlite3.h>

typedef struct {
  int enabled;
  const char *mbtiles;
} osm_config;

static void osm_register_hooks (apr_pool_t *p);
static int osm_handler(request_rec *r);
const char *osm_set_mbtiles_path(cmd_parms *cmd, void *cfg, const char *arg);
const char *osm_set_enabled(cmd_parms *cmd, void *cfg, const char *arg);
static int callback(void *r, int argc, char **argv, char **azColName);
static osm_config config;

static const command_rec osm_directives[] = {
  AP_INIT_TAKE1("osmEnabled", osm_set_enabled, NULL, RSRC_CONF, "Enable or disable mod_osm"),
  AP_INIT_TAKE1("osmMbtilesPath", osm_set_mbtiles_path, NULL, RSRC_CONF, "The path to osm db."),
  { NULL }
};

module AP_MODULE_DECLARE_DATA osm_module = {
  STANDARD20_MODULE_STUFF,
  NULL,
  NULL,
  NULL,
  NULL,
  osm_directives,
  osm_register_hooks 
};

const char *osm_set_enabled(cmd_parms *cmd, void *cfg, const char *arg) {
  if(!strcasecmp(arg, "true")) 
    config.enabled = 1;
  else 
    config.enabled = 0;
  return NULL;
}

const char *osm_set_mbtiles_path(cmd_parms *cmd, void *cfg, const char *arg) {
  config.mbtiles = arg;
  return NULL;
}

static void osm_register_hooks (apr_pool_t *p) { 
  config.mbtiles = "/tmp/0.mbtiles";
  ap_hook_handler(osm_handler, NULL, NULL, APR_HOOK_MIDDLE);
} 

static int readTile(sqlite3 *db, const int z, const int x, const int y, unsigned char **pTile, int *psTile ) {

  const char *sql = "SELECT tile_data from tiles where zoom_level=? and tile_column=? and tile_row=?;";;
  sqlite3_stmt *pStmt;
  int rc;

  *pTile = 0;
  *psTile = 0;

  do {
    rc = sqlite3_prepare(db, sql, -1, &pStmt, 0);
    if( rc!=SQLITE_OK ){
      return rc;
    }

    sqlite3_bind_int(pStmt, 1, z);
    sqlite3_bind_int(pStmt, 2, x);
    sqlite3_bind_int(pStmt, 3, y);
    
    rc = sqlite3_step(pStmt);
    if( rc==SQLITE_ROW ){
      *psTile = sqlite3_column_bytes(pStmt, 0);
      *pTile = (unsigned char *)malloc(*psTile);
      memcpy(*pTile, sqlite3_column_blob(pStmt, 0), *psTile);
    }

    rc = sqlite3_finalize(pStmt);

  } while( rc==SQLITE_SCHEMA );

  return rc;
}

static int osm_handler(request_rec *r) {
  if (!r->handler || config.enabled == 0 || strcmp(r->handler, "osm-handler")) return(DECLINED);

  sqlite3 *db;
  unsigned char *tile;
  char const *mbtilePath;
  int tileSize,rc,z,x,y;

  z = 0;
  x = 0;
  y = 0;

  // moche ... tres tres tres moche !
  sscanf(r->uri, "/%d/%d/%d.png", &z, &x, &y);

  // faire un test sur le rc
  rc = sqlite3_open_v2(config.mbtiles, &db, SQLITE_OPEN_READONLY, NULL);

  // là ! c'est chiadé comme truc !
  y = ((1 << z) - y - 1);

  if(SQLITE_OK!=readTile(db, z, x, y, &tile, &tileSize) ){
    sqlite3_close(db);
    return 500;
  }
  
  if(!tile){
    sqlite3_close(db);
    return 404;
  }

  /* Marche en 2.4.7 (unstable debian)
  apr_table_t *GET;
  ap_args_to_table(r, &GET);
  const char *getQuery;
  struct stat mbtilesStat;

  getQuery = apr_table_get(GET, "q");

  if(NULL != getQuery) {
    ap_set_content_type(r, "text/html");
    if(!strcmp(getQuery, "getinfo")) {
      stat(config.mbtiles, &mbtilesStat);

      ap_rprintf(r, "<h1>mod_osm - Information</h1><hr>");
      ap_rprintf(r, "uri : %s<br>", r->uri);
      ap_rprintf(r, "<hr>");
      ap_rprintf(r, "mbtiles path: %s<br>", config.mbtiles);
      ap_rprintf(r, "size: %lu octets<br>", mbtilesStat.st_size);
      ap_rprintf(r, "uid: %d<br>", mbtilesStat.st_uid);
      ap_rprintf(r, "gid: %d<br>", mbtilesStat.st_gid);
      ap_rprintf(r, "last access: %s<br>", ctime(&mbtilesStat.st_atime));
      ap_rprintf(r, "last modification: %s<br>", ctime(&mbtilesStat.st_mtime));
      ap_rprintf(r, "last status change: %s<br>", ctime(&mbtilesStat.st_ctime));

    } else
      return 404;
    return OK;
  }else{
    ap_set_content_type(r, "image/png");
    ap_rwrite(tile, tileSize, r);
    return OK;
  }
  */
  
  ap_set_content_type(r, "image/png");
  ap_rwrite(tile, tileSize, r);

  sqlite3_close(db);
  free(tile);

  return OK; 
}

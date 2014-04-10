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
  const char *to9mbtiles;
  const char *to14mbtiles;
  const char *zerombtiles;
} osm_config;

static void osm_register_hooks (apr_pool_t *p);
static int osm_handler(request_rec *r);
const char *osm_set_0mbtiles_path(cmd_parms *cmd, void *cfg, const char *arg);
const char *osm_set_1to9mbtiles_path(cmd_parms *cmd, void *cfg, const char *arg);
const char *osm_set_10to14mbtiles_path(cmd_parms *cmd, void *cfg, const char *arg);
static int callback(void *r, int argc, char **argv, char **azColName);
static osm_config config;

static const command_rec osm_directives[] = {
  AP_INIT_TAKE1("osmMbtiles0Path", osm_set_0mbtiles_path, NULL, RSRC_CONF, "The path to osm db."),
  AP_INIT_TAKE1("osmMbtiles1to9Path", osm_set_1to9mbtiles_path, NULL, RSRC_CONF, "The path to osm db."),
  AP_INIT_TAKE1("osmMbtiles10to14Path", osm_set_10to14mbtiles_path, NULL, RSRC_CONF, "The path to osm db."),
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

const char *osm_set_0mbtiles_path(cmd_parms *cmd, void *cfg, const char *arg) {
  config.zerombtiles = arg;
  return NULL;
}

const char *osm_set_1to9mbtiles_path(cmd_parms *cmd, void *cfg, const char *arg) {
  config.to9mbtiles = arg;
  return NULL;
}

const char *osm_set_10to14mbtiles_path(cmd_parms *cmd, void *cfg, const char *arg) {
  config.to14mbtiles = arg;
  return NULL;
}

static void osm_register_hooks (apr_pool_t *p) { 
  config.zerombtiles = "/tmp/0.mbtiles";
  config.to9mbtiles = "/tmp/osmfr-z1-z9.mbtiles";
  config.to14mbtiles = "/tmp/osmfr-z10-z14.mbtiles";
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
  if (!r->handler || strcmp(r->handler, "osm-handler")) return(DECLINED);

  sqlite3 *db;
  unsigned char *tile;
  char const *mbtilePath;
  int tileSize,rc,z,x,y;

  z = 0;
  x = 0;
  y = 0;

  // moche ... tres tres tres moche !
  sscanf(r->uri, "/%d/%d/%d.png", &z, &x, &y);

  if(z == 0)
    mbtilePath = config.zerombtiles;
  else if(z >= 1 && z <= 9)
    mbtilePath = config.to9mbtiles;
  else if(z >= 10 && z <= 14)
    mbtilePath = config.to14mbtiles;
  else 
    return 404;

  rc = sqlite3_open(mbtilePath, &db);
  
  if(SQLITE_OK!=readTile(db, z, x , y, &tile, &tileSize) ){
    sqlite3_close(db);
    return 500;
  }
  
  if(!tile){
    sqlite3_close(db);
    return 404;
  }

  ap_set_content_type(r, "image/png");
  ap_rwrite(tile, tileSize, r);

  sqlite3_close(db);
  free(tile);

  return OK; 
}

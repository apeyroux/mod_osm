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
  // OR_ALL : pour pouvoir mettre la cfg n'import où dans le vhost (dans un <Location> par ex)
  AP_INIT_TAKE1("osmEnabled", osm_set_enabled, NULL, OR_ALL, "Enable or disable mod_osm"),
  AP_INIT_TAKE1("osmMbtilesPath", osm_set_mbtiles_path, NULL, OR_ALL, "The path to osm db."),
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
  if(!apr_strnatcasecmp(arg, "true")) 
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
  config.enabled = 0;
  ap_hook_handler(osm_handler, NULL, NULL, APR_HOOK_FIRST);
} 

static int readTile(sqlite3 *db, const int z, const int x, const int y, unsigned char **pTile, int *psTile ) {

  const char *sql = "SELECT tile_data from tiles where zoom_level=? and tile_column=? and tile_row=?;";;
  sqlite3_stmt *pStmt;
  int rc;

  *pTile = NULL;
  *psTile = NULL;

  do {
    rc = sqlite3_prepare(db, sql, -1, &pStmt, 0);
    if(rc!=SQLITE_OK){
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

  } while(rc==SQLITE_SCHEMA);

  return rc;
}

static int osm_handler(request_rec *r) {

  //if (!r->handler || config.enabled == 0 || strcmp(r->handler, "osm-handler")) return(DECLINED);
  if (config.enabled == 0) return(DECLINED);

  sqlite3 *db;
  unsigned char *tile;
  int tileSize,z,x,y;

  db = NULL;
  tile = NULL;
  tileSize = 0;
  z = 0;
  x = 0;
  y = 0;

  /*
    TODO: moche ... tres tres tres moche !
    changer par ap_rxplus (apache 2.4)
    http://svn.apache.org/repos/asf/httpd/sandbox/replacelimit/include/ap_regex.h
  */

  sscanf(r->uri, "/%d/%d/%d.png", &z, &x, &y);

  // là ! c'est chiadé comme truc !
  if(y != 0)
    y = ((1 << z) - y - 1);

  if(SQLITE_OK!=sqlite3_open_v2(config.mbtiles, &db, SQLITE_OPEN_READONLY, NULL)) {
    sqlite3_close(db);
    ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "cnx mbtiles not possible.");
    return HTTP_INTERNAL_SERVER_ERROR;
  }

  if(SQLITE_OK!=readTile(db, z, x, y, &tile, &tileSize) ) {
    ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "impos to read tile in mbtiles.");
    sqlite3_close(db);
    return(DECLINED);
  }
  
  if(NULL == tile){
    sqlite3_close(db);
    ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, "tile %d/%d/%d.png not found", z, x, y);
    return(DECLINED);
  }

  ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, "get tile (size:%d) : %d/%d/%d.png", tileSize, z, x, y);
  
  ap_set_content_type(r, "image/png");  
  ap_rwrite(tile, tileSize, r);

  sqlite3_close(db);
  free(tile);

  return OK; 
}

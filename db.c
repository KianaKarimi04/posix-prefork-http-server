#include "db.h"
#include <fcntl.h>
#include <ndbm.h>
#include <stdio.h>
#include <string.h>

int store_data(const char *key, const char *value) {
  // ✅ FIX: removed O_TRUNC
  DBM *db = dbm_open("data.db", O_RDWR | O_CREAT, 0666);
  if (!db) {
    perror("dbm_open");
    return -1;
  }

  datum k, v;

  k.dptr = (char *)key;
  k.dsize = strlen(key);

  v.dptr = (char *)value;
  v.dsize = strlen(value);

  if (dbm_store(db, k, v, DBM_REPLACE) != 0) {
    perror("dbm_store");
    dbm_close(db);
    return -1;
  }

  dbm_close(db);
  return 0;
}
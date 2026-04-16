//
// Created by kiana on 3/29/26.
//
#include <fcntl.h>
#include <ndbm.h>
#include <stdio.h>

int main() {
  DBM *db = dbm_open("data.db", O_RDONLY, 0666);

  if (!db) {
    printf("Database not found or empty.\n");
    return 1;
  }

  datum key = dbm_firstkey(db);

  while (key.dptr) {
    datum value = dbm_fetch(db, key);

    printf("%.*s => %.*s\n", key.dsize, key.dptr, value.dsize, value.dptr);

    key = dbm_nextkey(db);
  }

  dbm_close(db);
}
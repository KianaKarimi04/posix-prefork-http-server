//
// Created by kiana on 3/29/26.
//

#include "db.h"
#include <ndbm.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

int store_data(const char *key, const char *value) {
    //DBM *db = dbm_open("data.db", O_RDWR, 0666);
    DBM *db = dbm_open("data.db", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (!db) {
        perror("dbm_open");
        return -1;   // 🔥 return error
    }

    datum k, v;
    k.dptr = (char*)key;
    k.dsize = strlen(key);

    v.dptr = (char*)value;
    v.dsize = strlen(value);

    if (dbm_store(db, k, v, DBM_REPLACE) != 0) {
        perror("dbm_store");
        dbm_close(db);
        return -1;   // 🔥 return error
    }

    dbm_close(db);
    return 0;   // ✅ success
}
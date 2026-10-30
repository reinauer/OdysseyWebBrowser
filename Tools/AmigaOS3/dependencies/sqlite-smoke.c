#include <sqlite3.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

#ifndef SQLITE_SMOKE_VOLUME
#define SQLITE_SMOKE_VOLUME "RAM:"
#endif
static const char* path = SQLITE_SMOKE_VOLUME "sqlite-smoke.db";
static int execute(sqlite3* db, const char* sql)
{
    char* message = NULL;
    int result = sqlite3_exec(db, sql, NULL, NULL, &message);
    if (result) printf("SQL result %d extended=%d DOS=%ld (%s): %s\n", result, sqlite3_extended_errcode(db), (long)IoErr(), sql, message ? message : "");
    sqlite3_free(message);
    return result;
}
static int value(sqlite3* db)
{
    sqlite3_stmt* query;
    if (sqlite3_prepare_v2(db, "SELECT n FROM sample", -1, &query, NULL)) return -1;
    int result = sqlite3_step(query) == SQLITE_ROW ? sqlite3_column_int(query, 0) : -1;
    sqlite3_finalize(query);
    return result;
}
int main(int argc, char** argv)
{
    sqlite3* db;
    int result;
    if (argc > 1 && !strcmp(argv[1], "--contender")) {
        if (sqlite3_open(path, &db)) return 20;
        result = execute(db, "BEGIN IMMEDIATE");
        sqlite3_close(db);
        if (result != SQLITE_BUSY) return 21;
        puts("PASS separate DOS process cannot acquire RESERVED lock");
        return 0;
    }
    DeleteFile(path);
    DeleteFile(SQLITE_SMOKE_VOLUME "sqlite-smoke.db-journal");
    result = sqlite3_open(path, &db);
    printf("sqlite open: %d %s\n", result, db ? sqlite3_errmsg(db) : "no handle");
    if (result) return 1;
    if (execute(db, "PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL; CREATE TABLE sample(n INTEGER); INSERT INTO sample VALUES(68)")) return 2;
    if (sqlite3_close(db) || sqlite3_open(path, &db) || value(db) != 68) return 3;
    puts("PASS SQLite commit, flush and reopen");
    if (execute(db, "BEGIN IMMEDIATE; UPDATE sample SET n=88")) return 4;
    result = SystemTags("SYS:odyssey-platform-test --contender >SYS:sqlite-contender.txt", TAG_DONE);
    printf("contender process result: %d\n", result);
    if (result) return 5;
    if (execute(db, "ROLLBACK") || sqlite3_close(db) || sqlite3_open(path, &db) || value(db) != 68) return 6;
    puts("PASS SQLite cross-process writer exclusion and rollback/reopen");
    if (execute(db, "BEGIN IMMEDIATE; UPDATE sample SET n=92; COMMIT")
        || sqlite3_close(db) || sqlite3_open(path, &db) || value(db) != 92) return 7;
    if (execute(db, "PRAGMA integrity_check")) return 8;
    sqlite3_close(db);
    sqlite3_vfs* vfs = sqlite3_vfs_find("amigaos3");
    sqlite3_file* file = sqlite3_malloc(vfs->szOsFile);
    int flags;
    result = vfs->xOpen(vfs, path, file, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_EXCLUSIVE | SQLITE_OPEN_MAIN_DB, &flags);
    sqlite3_free(file);
    if (result != SQLITE_CANTOPEN) return 9;
    BPTR guard = CreateDir(SQLITE_SMOKE_VOLUME "sqlite-smoke.db.sqlite-create-lock");
    if (!guard) return 10;
    UnLock(guard);
    result = sqlite3_open(path, &db);
    if (result != SQLITE_BUSY) return 11;
    sqlite3_close(db);
    BPTR preserved = Lock(SQLITE_SMOKE_VOLUME "sqlite-smoke.db.sqlite-create-lock", SHARED_LOCK);
    if (!preserved) return 12;
    UnLock(preserved);
    DeleteFile(SQLITE_SMOKE_VOLUME "sqlite-smoke.db.sqlite-create-lock");
    puts("PASS SQLite exclusive create and stale guard preservation");
    puts("PASS native SQLite smoke");
    return 0;
}

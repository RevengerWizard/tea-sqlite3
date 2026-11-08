/*
** SQlite3 library
** tea_sqlit3.c
*/

#include <string.h>
#include <sqlite3.h>

#define TEA_LIB
#include <tea.h>

#define DB_MT "sqlite3.Database"
#define STMT_MT "sqlite3.Statement"

typedef struct
{
    sqlite3* handle;
} Database;

typedef struct
{
    sqlite3_stmt* handle;
} Statement;

static void db_finalizer(void* p)
{
    Database* db = (Database*)p;
    if(db->handle) sqlite3_close_v2(db->handle);
    db->handle = NULL;
}

static void stmt_finalizer(void* p)
{
    Statement* stmt = (Statement*)p;
    if(stmt->handle) sqlite3_finalize(stmt->handle);
    stmt->handle = NULL;
}

static Database* db_check(tea_State* T)
{
    Database* db = (Database*)tea_check_udata(T, 0, DB_MT);
    if(!db->handle) tea_error(T, "database is closed");
    return db;
}

static Statement* stmt_check(tea_State* T)
{
    Statement* stmt = (Statement*)tea_check_udata(T, 0, STMT_MT);
    if(!stmt->handle) tea_error(T, "statement is finalized");
    return stmt;
}

static void db_error(tea_State* T, sqlite3* db, int rc)
{
    tea_error(T, "%s", db ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
}

static sqlite3_stmt* prepare(tea_State* T, sqlite3* db, int sqlarg)
{
    const char* sql = tea_check_string(T, sqlarg);
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK) db_error(T, db, rc);
    return stmt;
}

static void bind_value(tea_State* T, sqlite3_stmt* stmt, int slot, int index)
{
    int type = tea_get_type(T, index);
    int rc = SQLITE_OK;
    switch(type)
    {
    case TEA_TYPE_NIL: rc = sqlite3_bind_null(stmt, slot); break;
    case TEA_TYPE_BOOL: rc = sqlite3_bind_int(stmt, slot, tea_get_bool(T, index)); break;
    case TEA_TYPE_NUMBER: rc = sqlite3_bind_double(stmt, slot, tea_get_number(T, index)); break;
    case TEA_TYPE_STRING:
    {
        size_t len;
        const char* s = tea_get_lstring(T, index, &len);
        rc = sqlite3_bind_text(stmt, slot, s, (int)len, SQLITE_TRANSIENT);
        break;
    }
    default: tea_error(T, "unsupported SQLite parameter type: %s", tea_typeof(T, index));
    }
    if(rc != SQLITE_OK) db_error(T, sqlite3_db_handle(stmt), rc);
}

static void bind_params(tea_State* T, sqlite3_stmt* stmt, int index)
{
    int count = sqlite3_bind_parameter_count(stmt);
    if(tea_is_nonenil(T, index)) return;
    if(tea_is_list(T, index))
    {
        int n = tea_len(T, index);
        int i;
        for(i = 0; i < n && i < count; i++)
        {
            tea_get_item(T, index, i);
            bind_value(T, stmt, i + 1, -1);
            tea_pop(T, 1);
        }
        return;
    }
    tea_check_map(T, index);
    {
        int i;
        for(i = 1; i <= count; i++)
        {
            const char* name = sqlite3_bind_parameter_name(stmt, i);
            if(name && (name[0] == ':' || name[0] == '@' || name[0] == '$')) name++;
            if(name && tea_get_key(T, index, name))
            {
                bind_value(T, stmt, i, -1);
                tea_pop(T, 1);
            }
        }
    }
}

static void push_column(tea_State* T, sqlite3_stmt* stmt, int col)
{
    switch(sqlite3_column_type(stmt, col))
    {
    case SQLITE_INTEGER: tea_push_number(T, (tea_Number)sqlite3_column_int64(stmt, col)); break;
    case SQLITE_FLOAT: tea_push_number(T, sqlite3_column_double(stmt, col)); break;
    case SQLITE_TEXT:
        tea_push_lstring(T, (const char*)sqlite3_column_text(stmt, col), (size_t)sqlite3_column_bytes(stmt, col));
        break;
    case SQLITE_BLOB:
        tea_push_lstring(T, (const char*)sqlite3_column_blob(stmt, col), (size_t)sqlite3_column_bytes(stmt, col));
        break;
    default: tea_push_nil(T); break;
    }
}

static void push_row(tea_State* T, sqlite3_stmt* stmt)
{
    int i, n = sqlite3_column_count(stmt);
    tea_new_map(T);
    for(i = 0; i < n; i++)
    {
        push_column(T, stmt, i);
        tea_set_key(T, -2, sqlite3_column_name(stmt, i));
    }
}

static int step(tea_State* T, sqlite3_stmt* stmt)
{
    int rc = sqlite3_step(stmt);
    if(rc != SQLITE_ROW && rc != SQLITE_DONE) db_error(T, sqlite3_db_handle(stmt), rc);
    return rc;
}

static int run_sql(tea_State* T, sqlite3* db, int sqlarg, int params)
{
    sqlite3_stmt* stmt = prepare(T, db, sqlarg);
    int rc;
    if(!stmt) return SQLITE_DONE;
    if(params) bind_params(T, stmt, params);
    do { rc = step(T, stmt); } while(rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return rc;
}

static void db_exec(tea_State* T)
{
    Database* db = db_check(T);
    const char* sql = tea_check_string(T, 1);
    char* message = NULL;
    int rc = sqlite3_exec(db->handle, sql, NULL, NULL, &message);
    if(rc != SQLITE_OK)
    {
        const char* error = message ? message : sqlite3_errmsg(db->handle);
        tea_push_string(T, error);
        sqlite3_free(message);
        tea_throw(T);
    }
}

static void db_run(tea_State* T)
{
    Database* db = db_check(T);
    run_sql(T, db->handle, 1, tea_get_top(T) > 2 ? 2 : 0);
    tea_new_map(T);
    tea_push_integer(T, sqlite3_changes(db->handle)); tea_set_key(T, -2, "changes");
    tea_push_number(T, (tea_Number)sqlite3_last_insert_rowid(db->handle)); tea_set_key(T, -2, "last_id");
}

static void db_query(tea_State* T)
{
    Database* db = db_check(T);
    sqlite3_stmt* stmt = prepare(T, db->handle, 1);
    int rc;
    tea_new_list(T, 0);
    if(!stmt) return;
    if(tea_get_top(T) > 2) bind_params(T, stmt, 2);
    while((rc = step(T, stmt)) == SQLITE_ROW)
    {
        push_row(T, stmt);
        tea_add_item(T, -2);
    }
    sqlite3_finalize(stmt);
}

static void db_row(tea_State* T)
{
    Database* db = db_check(T);
    sqlite3_stmt* stmt = prepare(T, db->handle, 1);
    int rc;
    if(!stmt) { tea_push_nil(T); return; }
    if(tea_get_top(T) > 2) bind_params(T, stmt, 2);
    rc = step(T, stmt);
    if(rc == SQLITE_ROW) push_row(T, stmt); else tea_push_nil(T);
    sqlite3_finalize(stmt);
}

static void db_value(tea_State* T)
{
    Database* db = db_check(T);
    sqlite3_stmt* stmt = prepare(T, db->handle, 1);
    int rc;
    if(!stmt) { tea_push_nil(T); return; }
    if(tea_get_top(T) > 2) bind_params(T, stmt, 2);
    rc = step(T, stmt);
    if(rc == SQLITE_ROW && sqlite3_column_count(stmt)) push_column(T, stmt, 0);
    else tea_push_nil(T);
    sqlite3_finalize(stmt);
}

static void db_close(tea_State* T)
{
    Database* db = (Database*)tea_check_udata(T, 0, DB_MT);
    if(db->handle)
    {
        int rc = sqlite3_close(db->handle);
        if(rc != SQLITE_OK) db_error(T, db->handle, rc);
        db->handle = NULL;
    }
}

static void db_begin(tea_State* T) { Database* db = db_check(T); if(sqlite3_exec(db->handle, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) db_error(T, db->handle, sqlite3_errcode(db->handle)); }
static void db_commit(tea_State* T) { Database* db = db_check(T); if(sqlite3_exec(db->handle, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) db_error(T, db->handle, sqlite3_errcode(db->handle)); }
static void db_rollback(tea_State* T) { Database* db = db_check(T); if(sqlite3_exec(db->handle, "ROLLBACK", NULL, NULL, NULL) != SQLITE_OK) db_error(T, db->handle, sqlite3_errcode(db->handle)); }

static void db_transaction(tea_State* T)
{
    Database* db = db_check(T);
    tea_check_function(T, 1);
    if(sqlite3_exec(db->handle, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) db_error(T, db->handle, sqlite3_errcode(db->handle));
    tea_push_value(T, 1);
    if(tea_pcall(T, 0) != TEA_OK)
    {
        sqlite3_exec(db->handle, "ROLLBACK", NULL, NULL, NULL);
        tea_throw(T);
    }
    if(sqlite3_exec(db->handle, "COMMIT", NULL, NULL, NULL) != SQLITE_OK)
    {
        sqlite3_exec(db->handle, "ROLLBACK", NULL, NULL, NULL);
        db_error(T, db->handle, sqlite3_errcode(db->handle));
    }
}

static void db_prepare(tea_State* T)
{
    Database* db = db_check(T);
    sqlite3_stmt* raw = prepare(T, db->handle, 1);
    Statement* stmt = (Statement*)tea_new_udata(T, sizeof(Statement), STMT_MT);
    stmt->handle = raw;
    tea_set_finalizer(T, stmt_finalizer);
}

static void stmt_finalize(tea_State* T)
{
    Statement* stmt = (Statement*)tea_check_udata(T, 0, STMT_MT);
    if(stmt->handle) { sqlite3_finalize(stmt->handle); stmt->handle = NULL; }
}

static void stmt_run(tea_State* T)
{
    Statement* s = stmt_check(T);
    sqlite3_reset(s->handle); sqlite3_clear_bindings(s->handle);
    if(tea_get_top(T) > 1) bind_params(T, s->handle, 1);
    while(step(T, s->handle) == SQLITE_ROW) { }
    tea_new_map(T);
    tea_push_integer(T, sqlite3_changes(sqlite3_db_handle(s->handle))); tea_set_key(T, -2, "changes");
    tea_push_number(T, (tea_Number)sqlite3_last_insert_rowid(sqlite3_db_handle(s->handle))); tea_set_key(T, -2, "last_id");
}

static void stmt_query(tea_State* T)
{
    Statement* s = stmt_check(T);
    sqlite3_reset(s->handle); sqlite3_clear_bindings(s->handle);
    if(tea_get_top(T) > 1) bind_params(T, s->handle, 1);
    tea_new_list(T, 0);
    while(step(T, s->handle) == SQLITE_ROW) { push_row(T, s->handle); tea_add_item(T, -2); }
}

static const tea_Methods db_methods[] = {
    {"exec", "method", db_exec, 2, 0},
    {"run", "method", db_run, 2, 1},
    {"query", "method", db_query, 2, 1},
    {"row", "method", db_row, 2, 1},
    {"value", "method", db_value, 2, 1},
    {"prepare", "method", db_prepare, 2, 0},
    {"transaction", "method", db_transaction, 2, 0},
    {"begin", "method", db_begin, 1, 0},
    {"commit", "method", db_commit, 1, 0},
    {"rollback", "method", db_rollback, 1, 0},
    {"close", "method", db_close, 1, 0},
    {NULL, NULL, NULL, 0, 0}
};

static const tea_Methods stmt_methods[] = {
    {"run", "method", stmt_run, 1, 1},
    {"query", "method", stmt_query, 1, 1},
    {"finalize", "method", stmt_finalize, 1, 0},
    {NULL, NULL, NULL, 0, 0}
};

static void sqlite_open(tea_State* T)
{
    const char* path = tea_check_string(T, 0);
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    sqlite3* handle = NULL;
    Database* db;
    if(tea_get_top(T) > 1 && tea_is_map(T, 1) && tea_get_key(T, 1, "readonly"))
    {
        if(tea_to_bool(T, -1)) flags = SQLITE_OPEN_READONLY;
        tea_pop(T, 1);
    }
    {
        int rc = sqlite3_open_v2(path, &handle, flags, NULL);
        if(rc != SQLITE_OK)
        {
            const char* error = handle ? sqlite3_errmsg(handle) : sqlite3_errstr(rc);
            tea_push_string(T, error);
            if(handle) sqlite3_close(handle);
            tea_throw(T);
        }
    }
    db = (Database*)tea_new_udata(T, sizeof(Database), DB_MT);
    db->handle = handle;
    tea_set_finalizer(T, db_finalizer);
}

static const tea_Reg module_functions[] = {
    {"open", sqlite_open, 1, 1},
    {NULL, NULL, 0, 0}
};

TEAMOD_API void tea_import_sqlite3(tea_State* T)
{
    tea_create_class(T, DB_MT, db_methods);
    tea_set_key(T, TEA_REGISTRY_INDEX, DB_MT);
    tea_create_class(T, STMT_MT, stmt_methods);
    tea_set_key(T, TEA_REGISTRY_INDEX, STMT_MT);
    tea_create_module(T, "sqlite3", module_functions);
}

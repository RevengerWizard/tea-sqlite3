# `sqlite3` API

The `sqlite3` module wraps the SQLite C library. Import the module and open a database to get a `Database` value:

```tea
import sqlite3

const db = sqlite3.open("app.db")
```

The module currently provides `open`, the `Database` methods documented below, and a reusable `Statement` returned by `prepare`. SQLite errors are raised as Teascript runtime errors containing SQLite's error message. Use normal Teascript error handling (`pcall`, where applicable) around operations that can fail.

## Opening a database

```tea
sqlite3.open(path [, opts]) -> Database
```

`path` is a string passed to SQLite. Use `":memory:"` for a temporary in-memory database. If the file does not exist, the default mode creates it.

The optional map currently recognizes `readonly`:

```tea
const db = sqlite3.open("existing.db", {readonly = true})
```

When true, SQLite opens the database read-only. When false or omitted, the database opens read-write and is created if needed. Other option keys are ignored.

## Database

Database methods use the database as their receiver (`db.method(...)`).

| Method | Behavior |
| --- | --- |
| `exec(sql)` | Executes one or more SQL statements. Discards result rows and returns no value. Use for schema setup and scripts without parameters. |
| `run(sql [, params])` | Executes a statement, consuming any returned rows. Returns a map with `changes` and `last_id`. |
| `query(sql [, params])` | Executes a query and returns a list containing every row. Each row is a map from column name to value. |
| `row(sql [, params])` | Returns the first row as a map, or `nil` if there is no row. |
| `value(sql [, params])` | Returns the first column of the first row, or `nil` if there is no row or column. |
| `prepare(sql)` | Prepares a reusable `Statement` from the first SQL statement in `sql`. |
| `transaction(fn)` | Runs `fn()` between `BEGIN` and `COMMIT`. If `fn` raises an error, rolls back and re-raises it. |
| `begin()` | Begins a transaction. |
| `commit()` | Commits the current transaction. |
| `rollback()` | Rolls back the current transaction. |
| `close()` | Closes the database. Repeated calls are safe; later database operations raise an error. |

`run` returns the number of rows changed by the most recent statement and the most recent inserted row ID:

```tea
const result = db.run("insert into users(name) values (?)", ["Ada"])
print(result.changes)
print(result.last_id)
```

`exec` is intended for SQL that does not need bound parameters or returned rows. `run`, `query`, `row`, and `value` prepare and execute only the first statement in their SQL string. Use `exec` when a string contains multiple statements.

### Parameters

Pass parameters as either a list for positional placeholders or a map for named placeholders. Supported placeholders are SQLite's `?`, `:name`, `@name`, and `$name` forms:

```tea
db.run("INSERT INTO users(name, age) VALUES (?, ?)", ["Ada", 37])
db.run("INSERT INTO users(name, age) VALUES (:name, :age)",
       {name = "Grace", age = 85})

const adults = db.query("SELECT name FROM users WHERE age >= ?", [18])
const user = db.row("SELECT * FROM users WHERE name = :name", {name = "Ada"})
const count = db.value("SELECT count(*) FROM users")
```

For named placeholders, map keys omit the placeholder prefix: use `name` for `:name`, `@name`, or `$name`. Positional values are matched to bind slots in order. Missing named values and omitted positional values are left unbound, so SQLite treats them as `NULL`. Extra positional values are ignored.

### Transactions

Use `transaction` to commit automatically when its callback completes and roll back if the callback raises an error:

```tea
db.transaction(function() {
    db.run("INSERT INTO accounts(name) VALUES (?)", ["checking"])
    db.run("INSERT INTO accounts(name) VALUES (?)", ["savings"])
})
```

For manual control, call `begin`, then either `commit` or `rollback`. These methods use SQLite's default transaction behavior and do not accept options.

## Statement

`db.prepare(sql)` returns a reusable `Statement`. Its methods bind parameters for the next execution, reset the statement before it runs, then clear bindings on the following execution:

| Method | Behavior |
| --- | --- |
| `run([params])` | Executes the prepared statement and returns `{changes, last_id}`. |
| `query([params])` | Executes the prepared statement and returns all rows as a list of maps. |
| `finalize()` | Releases the prepared statement. Repeated calls are safe; later use raises an error. |

Example:

```tea
const insert = db.prepare("INSERT INTO logs(message) VALUES (?)")
insert.run(["started"])
insert.run(["finished"])
insert.finalize()
```

Database and statement resources are also finalized when their Teascript userdata is collected. Close or finalize explicitly when you need the resource released at a specific point.

## Type conversion

SQLite values are converted to Teascript values as follows:

| SQLite storage class | Teascript value | Details |
| --- | --- | --- |
| `NULL` | `nil` | |
| `INTEGER` | `number` | SQLite's 64-bit integer is converted to Teascript's number type. Very large integers may not be exactly representable. |
| `REAL` | `number` | |
| `TEXT` | `string` | Length-aware conversion preserves embedded NUL bytes. |
| `BLOB` | `string` | Bytes are returned as a Teascript string. |

When binding parameters, `nil`, booleans, numbers, and strings are supported. Booleans bind as integer `0` or `1`; numbers bind as SQLite floating-point values. A string parameter binds as SQLite `TEXT`. There is no distinct Teascript value for binding a BLOB in the current API.

## Current limitations

- `each(sql [, params])` is not implemented yet; use `query` to retrieve rows.
- `exec` does not bind parameters or return rows.
- `prepare` prepares the first statement in the supplied SQL string.
- The optional `open` map currently supports only `readonly`.

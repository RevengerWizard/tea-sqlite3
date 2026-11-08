# tea-sqlite3

`tea-sqlite3` is a Teascript module backed by the SQLite C library. The `lib/` directory contains the SQLite amalgamation, and `src/tea_sqlite3.c` implements the Teascript-facing API using the public Teascript C API in `tea.h`.

The module is loaded from `sqlite3.dll` through the exported `tea_import_sqlite3` entry point. See [api.md](api.md) for the module API, examples, type conversions, and current limitations.

## Requirements

- A C compiler supported by the project Makefile (GCC by default)
- GNU Make
- A built Teascript runtime, including its public `tea.h` header and link library

## Build

Set `TEA_SRC` to the Teascript source directory that directly contains `tea.h`. The build places the resulting `sqlite3.dll` in the current project directory.

For example, from the `tea-sqlite3` directory:

```sh
make TEA_SRC=/path/to/teascript/src
```

If `TEA_SRC` is already exported in your environment, run:

```sh
make
```

The SQLite amalgamation is compiled as part of the module; a separate SQLite library is not required.

To remove the generated DLL and object files:

```sh
make clean
```

## Install

The interpreter searches its module library directory for `sqlite3.dll`. Copy the built DLL into `$(TEA_SRC)/lib` (or the library directory used by your Teascript installation):

The Makefile provides an optional `install` target:

```sh
make install TEA_SRC=/path/to/teascript/src
```

## Quick start

```tea
import sqlite3

const db = sqlite3.open("example.db")
db.exec("CREATE TABLE IF NOT EXISTS notes (id integer primary key, body text)")
db.run("INSERT INTO notes(body) VALUES (?)", ["Hello, SQLite"])

const notes = db.query("SELECT id, body FROM notes")
for const note in notes
{
    print(note.id, note.body)
}

db.close()
```

For named parameters, pass a map whose keys omit the SQLite placeholder prefix:

```tea
db.run("INSERT INTO notes(body) VALUES (:body)", {body = "A named value"})
```

## Documentation

- [API reference](api.md)
- [SQLite](https://www.sqlite.org/)
- [Teascript](https://teascript.org/)

## License

Licenced under MIT License. [Copy of the license can be found here](https://github.com/RevengerWizard/tea_sqlite3/blob/master/LICENSE)

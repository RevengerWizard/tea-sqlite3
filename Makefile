# Set TEA_SRC to the Teascript source directory (the one containing src/tea.h).
CC ?= gcc
TEA_SRC ?= ..\teascript
CFLAGS ?= -O2 -Wall
CFLAGS += -fPIC -DTEA_BUILD_AS_DLL -I"$(TEA_SRC)" -Ilib
LDFLAGS ?= -shared
LIBS ?= -ltea00

TARGET = sqlite3.dll
STRIP = strip --strip-unneeded

OBJS = src/tea_sqlite3.o lib/sqlite3.o

.PHONY: all clean install

Q = @
E = @echo

all: $(TARGET)

$(TARGET): $(OBJS)
	$(E) "LINK          $@"
	$(Q)$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	$(E) "STRIP         $@"
	$(Q)$(STRIP) $@

src/tea_sqlite3.o: src/tea_sqlite3.c lib/sqlite3.h
	$(E) "CC            $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

lib/sqlite3.o: lib/sqlite3.c lib/sqlite3.h
	$(E) "CC            $@"
	$(Q)$(CC) $(CFLAGS) -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -c -o $@ $<

install: $(TARGET)
	$(E) "MOVE          $@  ->  src/lib"
	$(Q)cp $(TARGET) "$(TEA_SRC)/lib/sqlite3.dll"

clean:
	rm -f $(TARGET) src/tea_sqlite3.o lib/sqlite3.o

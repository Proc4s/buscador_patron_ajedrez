
CC=gcc
CFLAGS= -O3 -DSQLITE_THREADSAFE=0
LDFLAGS= -lsqlite3 -ldl -lm -lc

proy:  pruInserPartSqlite  pruSelSqlite

pruInserPartSqlite : pruInserPartSqlite.c
	$(CC) $(CFLAGS) -o pruInserPartSqlite pruInserPartSqlite.c $(LDFLAGS)
	
pruSelSqlite : pruSelSqlite.c
	$(CC) $(CFLAGS) -o pruSelSqlite pruSelSqlite.c $(LDFLAGS)



clean:
	rm pruInserPartSqlite pruSelSqlite


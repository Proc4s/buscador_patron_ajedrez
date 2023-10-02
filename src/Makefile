
CC=gcc
CFLAGS= -O3 -DSQLITE_THREADSAFE=0
LDFLAGS= -lsqlite3 -ldl -lm -lc

proy:  ../bin/mapbpatronsql  ../bin/genbasfich ../bin/fich2sqlite ../bin/gpatronbin ../bin/creabaseSqlite ../bin/sellistapart

../bin/lpartbase : lpartbase.c
	$(CC) $(CFLAGS) -o ../bin/lpartbase lpartbase.c $(LDFLAGS)
	
../bin/reduce : reduce.c config.o
	$(CC) $(CFLAGS) -o ../bin/reduce reduce.c config.o -lc -lrt
	
../bin/monitor : monitor.c config.o
	$(CC) $(CFLAGS) -o ../bin/monitor monitor.c config.o -lc -lrt
	
../bin/gpatronbin : gpatronbin.c ajedrez.h
	$(CC) $(CFLAGS) -o ../bin/gpatronbin gpatronbin.c  -lc

../bin/mapbpatronsql : mapbpatronsql.c ajedrez.h sqlitedrv.o funaux.o config.o
	$(CC) $(CFLAGS) -o ../bin/mapbpatronsql mapbpatronsql.c sqlitedrv.o funaux.o config.o $(LDFLAGS)
	
../bin/creabaseSqlite : creabaseSqlite.c
	$(CC) $(CFLAGS) -o ../bin/creabaseSqlite creabaseSqlite.c $(LDFLAGS)

../bin/sellistapart : sellistapart.c
	$(CC) $(CFLAGS) -o ../bin/sellistapart sellistapart.c $(LDFLAGS)
	
../bin/mapbpatronfich : mapbpatronfich.c ajedrez.h basfichdrv.o funaux.o config.o
	$(CC) $(CFLAGS) -o ../bin/mapbpatronfich mapbpatronfich.c basfichdrv.o funaux.o config.o -lc
	
../bin/fich2sqlite : fich2sqlite.c ajedrez.h sqlitedrv.o basfichdrv.o config.o
	$(CC) $(CFLAGS) -o ../bin/fich2sqlite fich2sqlite.c sqlitedrv.o basfichdrv.o config.o $(LDFLAGS)
	
../bin/genbasfich : genbasfich.c ajedrez.h basfichdrv.o
	$(CC) $(CFLAGS) -o ../bin/genbasfich genbasfich.c basfichdrv.o -lc

sqlitedrv.o : sqlitedrv.c ajedrez.h sqlitedrv.h
	$(CC) $(CFLAGS) -c -o sqlitedrv.o sqlitedrv.c
	
funaux.o : funaux.c funaux.h ajedrez.h
	$(CC) $(CFLAGS) -c -o funaux.o funaux.c
	
config.o : config.c config.h ajedrez.h
	$(CC) $(CFLAGS) -c -o config.o config.c

basfichdrv.o : basfichdrv.c ajedrez.h basfichdrv.h
	$(CC) $(CFLAGS) -c -o basfichdrv.o basfichdrv.c

clean:
	rm *.o
	rm ../bin/*

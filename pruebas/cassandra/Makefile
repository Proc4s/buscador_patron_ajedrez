
CC=gcc
CFLAGS= -O3
LDFLAGS= -lcassandra -lc

proy:  pruinserPartCass  pruSelCass

pruinserPartCass : pruinserPartCass.c
	$(CC) $(CFLAGS) -o pruinserPartCass pruinserPartCass.c $(LDFLAGS)
	
pruSelCass : pruSelCass.c
	$(CC) $(CFLAGS) -o pruSelCass pruSelCass.c $(LDFLAGS)



clean:
	rm pruinserPartCass pruSelCass


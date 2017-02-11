CFLAGS = -g -Wall 
CC     = gcc

ALL_D    = daemon.c log.c util.c lockfile.c socket.c confdata.c daemon-child-func.c

daemon: $(ALL_D)
	$(CC) -o daemon $(CFLAGS) $(ALL_D)

.PHONY: clean
clean:
	-rm -f *.o daemon \#*\# *~ logfile

#***********************************************************************
#
# Makefile
#
#***********************************************************************
CC = mipsel-linux-gcc

#CFLAGS = -g -rdynamic -DMYDEBUG -DDEBUG
LIBS = -lpthread

#all: asuswebstorage inotify
all: inotify

#https: https.o md5.o urlencode.o oauth.o oauth_http.o xmalloc.o
#asuswebstorage: asuswebstorage.o api.o function.o 
	#$(CC) $(CFLAGS) $(LIBS)  $^ -o $@ 

inotify: event_queue.o inotify_utils.o inotify.o
	$(CC) $(CFLAGS) $(LIBS)  $^ -o $@


.PHONY: clean
clean:
	-rm -rf *.o inotify

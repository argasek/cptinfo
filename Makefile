DEBUG=yes

BINNAME=cptinfo

ifeq ($(DEBUG),yes)
CC=gcc -O0 -g -std=c99 -lm
else
CC=gcc -O2 -march=athlon-4 -pipe -momit-leaf-frame-pointer -fomit-frame-pointer -fno-ident -std=c99 -lm
endif
GLIBCFLAGS=`pkg-config --cflags --libs glib-2.0`

default: $(BINNAME)

$(BINNAME): cptinfo.c cpt.h cpt6.h Makefile
	$(CC) $(GLIBCFLAGS) cptinfo.c -o $(BINNAME)
ifeq ($(DEBUG),no)
	strip $(BINNAME)
endif

clean:
	rm -f $(BINNAME)

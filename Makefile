# remove -g and add back in 
CFLAGS	= -D_DEFAULT_SOURCE -std=c11 -Wall -Wextra -flto -O3 -s -I/usr/include/SDL2
LDFLAGS	= -flto
LIBS	= -ljansson -lSDL_ILBM -lSDL2

ilbm2frames: ilbm2frames.c
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f ilbm2frames

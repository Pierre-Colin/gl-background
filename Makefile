CC=cc
SRC=wave.c glx.c
OBJ=${SRC:.c=.o}
CFLAGS=-std=c99 -pedantic -Wall -g -O0

gl: ${OBJ}
	@echo "LD $@"
	@${CC} $^ -o $@ -lGLEW -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -lm

new: new.o
	@echo "LD $@"
	@${CC} $^ -o $@ -lGLEW -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -lm

glx: glx.o
	@echo "LD $@"
	@${CC} $^ -o $@ -lGLEW -lglfw -lGL -lX11 -lpthread -lXrandr -lXrender -lXi -lm

glxnew: glxnew.o
	@echo "LD $@"
	@${CC} $^ -o $@ -lGLEW -lglfw -lGL -lX11 -lpthread -lXrandr -lXrender -lXi -lm

.c.o:
	@echo "CC $@"
	@${CC} ${CFLAGS} -c $<

clean:
	@echo "cleaning..."
	@rm -f gl ${OBJ}

.PHONY: clean

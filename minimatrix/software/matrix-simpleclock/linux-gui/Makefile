FLAGS= -Wall -ggdb -DMENU_MOUSE_SUPPORT
CC=gcc
#CC=/opt/gccgraph/bin/gcc
LIBS=
all:
	$(CC) -c ../menu-interpreter.c $(FLAGS) -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable
	$(CC) -c ../menu-text.c $(FLAGS) -Wextra
	$(CC) -c ../gui.c $(FLAGS) -DPC_BUILD -Wextra
	$(CC) -c ../timeconvert.c $(FLAGS) -DPC_BUILD -Wextra
	$(CC) -c demo-main.c $(FLAGS) -Wextra -Wno-unused-parameter
	$(CC) -o demo-main demo-main.o gui.o menu-interpreter.o menu-text.o timeconvert.o $(LIBS) -ggdb -lglut -lGL

debug:
	$(CC) -c ../menu-interpreter.c $(FLAGS) -DDEBUG
	$(CC) -c ../menu-text.c $(FLAGS) -DDEBUG -Wextra
	$(CC) -c ../gui.c $(FLAGS) -DPC_BUILD -DDEBUG -Wextra
	$(CC) -c ../timeconvert.c $(FLAGS) -DPC_BUILD -DDEBUG -Wextra
	$(CC) -c demo-main.c $(FLAGS) -DDEBUG -Wextra
	$(CC) -o demo-main demo-main.o gui.o menu-interpreter.o menu-text.o timeconvert.o $(LIBS) -ggdb -lglut -lGL

clean:
	rm -f *.o
	rm -f demo-main

removebackups:
	rm -f *~

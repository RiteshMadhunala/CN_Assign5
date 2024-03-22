init: all
	gcc -o init initmsocket.c -L. -lmsocket

all: libmsocket.a


libmsocket.a: msocket.o initmsocket.o
	ar rcs libmsocket.a msocket.o 

msocket.o: msocket.c
	gcc -c msocket.c

initmsocket.o: initmsocket.c
	gcc -c initmsocket.c

user1:  
	gcc -o user1 user1.c -L. -lmsocket

user2:
	gcc -o user2 user2.c -L. -lmsocket

clean:
	rm -f init user1 user2 initmsocket.o msocket.o libmsocket.a

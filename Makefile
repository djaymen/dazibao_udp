HEADERS = biblio.h

all: projet

projet:   main.o biblio.o client.o server.o
			gcc  -o projet    main.o biblio.o client.o server.o

main: main.o biblio.o
		gcc -o main main.o biblio.o

server: server.o
		gcc -o server server.o

client: client.o
		gcc -o client client.o

main.o: main.c biblio.c $(HEADERS) 
			gcc -o main.o -c main.c 

server.o: server.c $(HEADERS)
			gcc -o server.o -c server.c

client.o: client.c $(HEADERS)
			gcc -o client.o -c client.c

biblio.o: biblio.c 
			gcc -o biblio.o -c biblio.c

clean:
		rm -rf *.o

clean_all: clean
		rm -rf projet client server main
		
		
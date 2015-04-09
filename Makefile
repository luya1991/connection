
all: Client Server

Client: Client.o Error.o ClientDemo.cpp
	g++ -Wall ClientDemo.cpp Client.o Error.o -o Client -g

Server: Server.o Error.o ServerDemo.cpp
	g++ -Wall ServerDemo.cpp Server.o Error.o -o Server -g

Client.o: Client.cpp Client.h Headers.h
	g++ -Wall -I . -c Client.cpp -o Client.o -g
	
Server.o: Server.cpp Server.h Headers.h
	g++ -Wall -I . -c Server.cpp -o Server.o -g
	
Error.o: Error.cpp Error.h Headers.h
	g++ -Wall -I . -c Error.cpp -o Error.o -g

clean:
	find *.o | xargs -n 1 rm -f
	rm -f Client Server

server: server.o
	g++ server.o -o server -lwsock32 -lws2_32

server.o: server.cpp
	g++ -c server.cpp

clean:
	del server.exe server.o

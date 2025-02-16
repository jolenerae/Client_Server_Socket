client: client.o
	g++ client.o -o client -lwsock32 -lws2_32

client.o: client.cpp
	g++ -c client.cpp

clean:
	del client.exe client.o

#undef UNICODE

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501

#include <iostream>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>


// link with Ws2_32.lib
#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "Mswsock.lib" )

// define default values
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "54000"
#define NUM_THREADS 2

// global mutex for thread synchronization
CRITICAL_SECTION cf;

// three types of packets can be sent
enum MessageType {
    PING_QUESTION, // 0
    PING_ACCEPT, // 1
    PING_REJECT, // 2
    QUIT, // 3
    MESSAGE // 4
};

// create struct for packets
struct packet {
    // define variables carried in packet
    int packet_type;
    char message[DEFAULT_BUFLEN];
};

DWORD WINAPI thread_send(LPVOID Param);
DWORD WINAPI thread_receive(LPVOID Param);
bool check_key_timeout(char key, int timeout);
// methods to serialize and deserialize the pact structure
void serialize(char *buffer, packet *packet_msg);
void deserialize(char *buffer, packet *packet_msg);

int __cdecl main(void) 
{
    // structure that has info about winsocks
    WSADATA wsaData;
    int iResult;

    // define sockets
    SOCKET server_socket = INVALID_SOCKET;
    SOCKET client_socket = INVALID_SOCKET;

    struct addrinfo *result = NULL; // pointer to a struct to hold response info from host
    struct addrinfo hints; // struct to hold info about the server

    //create thread variables
    DWORD ThreadId;
    DWORD ThreadIdTwo;
    HANDLE ThreadHandle;
    HANDLE ThreadHandleTwo;
    // array to hold the threads
    HANDLE Threads[NUM_THREADS];

    // initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup error: " << iResult;
        return -1;
    }
    std::cout << "Winsock initialized\n";
    // clear the hint structure of any unwanted data
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; // for tcp message passing
    // add my own protocol here
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // translates the host name to an address, addrinfo struct returned to result
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
        std::cerr << "getaddrinfo error: " << iResult;
        WSACleanup();
        return -2;
    }

    // Create a SOCKET for the server to listen for client connections.
    server_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "socket error: " << WSAGetLastError();
        freeaddrinfo(result);
        WSACleanup();
        return -3;
    }
    std::cout << "Server socket created\n";

    // Setup the TCP listening socket
    iResult = bind(server_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "bind error: " << WSAGetLastError();
        freeaddrinfo(result);
        closesocket(server_socket);
        WSACleanup();
        return -4;
    }

    // free this space in memory
    freeaddrinfo(result);

    iResult = listen(server_socket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "listen error: " << WSAGetLastError();
        closesocket(server_socket);
        WSACleanup();
        return -5;
    }

    // Accept a client socket
    client_socket = accept(server_socket, NULL, NULL);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "accept client error: " << WSAGetLastError();
        closesocket(server_socket);
        WSACleanup();
        return -6;
    }

    std::cout << "Client is connected!\n";

    // No longer need server socket
    closesocket(server_socket);

    // now messages can be exchanged

    if (!InitializeCriticalSectionAndSpinCount(&cf, 0x00000400)) {
        return -1;
    }

    // create threads
    ThreadHandleTwo = CreateThread(NULL, 0, thread_send, &client_socket, 0, &ThreadIdTwo);
    ThreadHandle = CreateThread(NULL, 0, thread_receive, &client_socket, 0, &ThreadId);
    
    // organize into an array
    Threads[0] = ThreadHandle;
    Threads[1] = ThreadHandleTwo;

    HANDLE* Threads_ptr = Threads;

    if (Threads != NULL) {
        // wait for both threads to finish
        WaitForMultipleObjects(NUM_THREADS, Threads_ptr, true, INFINITE);

        for (int i; i < NUM_THREADS; i++) {
           CloseHandle(Threads[i]); 
        }
        
    }

    // clean up mutex
    DeleteCriticalSection(&cf);

    // shutdown the connection since we're done
    iResult = shutdown(client_socket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "shutdown failed with error: " << WSAGetLastError();
        closesocket(client_socket);
        WSACleanup();
        return -9;
    }

    // cleanup
    closesocket(client_socket);
    WSACleanup();

    return 0;
}

DWORD WINAPI thread_receive(LPVOID Param) {
    EnterCriticalSection(&cf);
    std::cout << "Receive thread starting\n";
    LeaveCriticalSection(&cf);
    SOCKET client_socket = *(SOCKET*)Param;
    char buffer[DEFAULT_BUFLEN];
    while(true) {
        ZeroMemory(buffer, sizeof(buffer));
        int iResult = recv(client_socket, buffer, sizeof(buffer), 0);
        std::cout << "test\n";
        // if message received
        if (iResult > 0) {
            // if receiving ping
            if (strcmp(buffer, ":ping") == 0) {
                EnterCriticalSection(&cf);
                std::cout << "Client sent a ping, are you online? Press Y to affirm.\n";
                LeaveCriticalSection(&cf);
                // if 'Y' is pressed on the keyboard within 5 minutes
                if (check_key_timeout('Y', 300000)) {
                    // send back a ping packet
                    ZeroMemory(buffer, sizeof(buffer));
                    strcpy(buffer, ":ping_accept");
                    EnterCriticalSection(&cf);
                    int iSendResult = send(client_socket, buffer, sizeof(buffer), 0);
                    LeaveCriticalSection(&cf);
                    if (iSendResult == SOCKET_ERROR) {
                        std::cerr << "send message failed with error: " << WSAGetLastError();
                        closesocket(client_socket);
                        WSACleanup();
                        return -7;
                    }
                } else {
                    ZeroMemory(buffer, sizeof(buffer));
                    strcpy(buffer, ":ping_reject");
                    int iSendResult = send(client_socket, buffer, sizeof(buffer), 0);
                    if (iSendResult == SOCKET_ERROR) {
                        std::cerr << "send message failed with error: " << WSAGetLastError();
                        closesocket(client_socket);
                        WSACleanup();
                        return -7;
                    }
                } 
            }
            // if receive quit packet
            else if (strcmp(buffer, ":quit") == 0) {
                EnterCriticalSection(&cf);
                std::cout << "Client is quitting, would you like to quit as well? (Y/N)\n";
                char response[DEFAULT_BUFLEN];
                std::cin >> response;
                LeaveCriticalSection(&cf);
                if (strcmp(response, "Y")) {
                    closesocket(client_socket); 
                    WSACleanup();
                    return 0;
                } else {
                    continue;
                }
            } else {   
                // if receiving msg
                // output message to screen
                EnterCriticalSection(&cf);
                std::cout << buffer << "\n";
                LeaveCriticalSection(&cf);
            }
        } else if (iResult < 0) {
            std::cerr << "receive message failed with error: " << WSAGetLastError();
            closesocket(client_socket);
            WSACleanup();
            return -7;
        }
        // if no message received just continue in the loop 
    }
    return 0;
}

DWORD WINAPI thread_send(LPVOID Param) {
    EnterCriticalSection(&cf);
    std::cout << "Send thread starting\n";
    LeaveCriticalSection(&cf);
    SOCKET client_socket = *(SOCKET*)Param;
    char buffer[DEFAULT_BUFLEN];
    while(true) {
        ZeroMemory(buffer, sizeof(buffer));
        EnterCriticalSection(&cf);
        std::cout << "> " << std::endl;
        // direct input into the buffer
        std::cin >> buffer;
        LeaveCriticalSection(&cf);
        // if sending ping packet
        if (strcmp(buffer, ":ping") == 0) {
            int iSendResult = send(client_socket, buffer, sizeof(buffer), 0);
            if (iSendResult == SOCKET_ERROR) {
                std::cerr << "send message failed with error: " << WSAGetLastError();
                closesocket(client_socket);
                WSACleanup();
                return -7;
            }
        } // if sending a quit packet
         else if (strcmp(buffer, ":quit") == 0) {
            char response[DEFAULT_BUFLEN];
            // output a are you sure message
            EnterCriticalSection(&cf);
            std::cout << "Are you sure you would like to quit? (Y/N)\n";
            std::cin >> response;
            LeaveCriticalSection(&cf);
            // if sure
            if (strcmp(response, "Y") == 0) {
                // send the quit packet
                int iSendResult = send(client_socket, buffer, sizeof(buffer), 0);
                if (iSendResult == SOCKET_ERROR) {
                    std::cerr << "send message failed with error: " << WSAGetLastError();
                    closesocket(client_socket);
                    WSACleanup();
                    return -7;
                }
                closesocket(client_socket);
                WSACleanup();
                return 0;
            } else { // response is N
                continue;
            }
        } else {
            // if sending msg packet
            int iSendResult = send(client_socket, buffer, sizeof(buffer), 0);
            if (iSendResult == SOCKET_ERROR) {
                std::cerr << "send message failed with error: " << WSAGetLastError();
                closesocket(client_socket);
                WSACleanup();
                return -7;
            }
        }
    }
    return 0;
}

// helper method for ping packet communication
bool check_key_timeout(char key, int timeout) {
    int elapsedTime = 0;
    // it will check every 100ms if key was pressed
    const int interval = 100;

    while (elapsedTime < timeout) {
        // check if a key has been pressed
        if (_kbhit()) {
            char input = _getch();
            // check for key and lower case key
            if (input == key || input == key + 32) {
                return true;
            }
        }
        // sleep and update time
        Sleep(interval);
        elapsedTime += interval;
    }
    // timeout was reached so key was not pressed
    return false;
}

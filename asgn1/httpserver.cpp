// Author: Anthony Ho
// CruzID: Ankho

#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#define defaultPort (char*)"80"

// Function to handle GET requests
void handleGet(int socket, char* resName) {
    char validName[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-_";
    if(strlen(resName) == 27 && strspn(resName,validName) == 27) {
        int file = open(resName, O_RDONLY);
        if(access(resName,0) == 0 && file == -1) {
            char* forbidden = (char*)"HTTP/1.1 403 FORBIDDEN\r\n";
            char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
            write(socket, forbidden, strlen(forbidden));
            write(socket,contentLength,strlen(contentLength));
        } else {
            int length = 0;
            if(file > 0) {
                char* buffer = (char*)malloc(32768);
                bool readComplete = false;
                while(!readComplete) {
                    int readFile = read(file,buffer,32768);
                    length += readFile;
                    if(readFile == 0) {
                        readComplete=true;
                    }
                }
                free(buffer);
                char* ok = (char*)"HTTP/1.1 200 OK\r\n";
                char* contentLength = (char*)"Content-Length: ";
                write(socket,ok,strlen(ok));                
                write(socket,contentLength,strlen(contentLength));
                char lenStr[12]; //max num of digits for a 32 bit integer is 12
                sprintf(lenStr, "%d\r\n\r\n", length);
                write(socket,lenStr, strlen(lenStr));
                //file = open(resName, O_RDONLY);
                lseek(file,0,0);
                char* nextBuffer = (char*)malloc(32768);
                readComplete = false;
                while(!readComplete) {
                    int readFile = read(file,nextBuffer,32768);
                    if(readFile == 0) {
                        readComplete=true;
                    }  else {
                        write(socket,nextBuffer,readFile);
                    }
                }
                free(nextBuffer);
            } else {
                char* notFound = (char*)"HTTP/1.1 404 NOT FOUND\r\n";
                char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
                write(socket,notFound,strlen(notFound));
                write(socket,contentLength,strlen(contentLength));
            }
        }
    } else {
            char* badReq = (char*)"HTTP/1.1 400 BAD REQUEST\r\n";
            char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
            write(socket,badReq,strlen(badReq));
            write(socket,contentLength,strlen(contentLength));
    }
}

// Function to handle PUT requests
void handlePut(int socket, char* resName, char* length) {
    char validName[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-_";
    int contentLen = 0;
    if(strcmp(length,"") != 0) {
        contentLen = atoi(length);
    }
    if(strlen(resName) == 27 && strspn(resName,validName) == 27) {
        if(open(resName, O_RDONLY) == -1) {
            int file = open(resName,O_CREAT | O_RDWR, 0666);
            char* newBuff = (char*)malloc(32768);
            if(contentLen > 0) {
                while(contentLen > 0) {
                    int clientFile = read(socket,newBuff,32768);
                    write(file, newBuff, clientFile);
                    contentLen -= clientFile;
                }
            } else {
                bool readComplete = false;
                while(!readComplete) {
                    int clientFile = read(socket,newBuff,32768);
                    if(clientFile == 0) {
                        readComplete = true;
                    }
                    write(file, newBuff, clientFile);
                }
            }
            free(newBuff);
            char* created = (char*)"HTTP/1.1 201 CREATED\r\n";
            char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
            write(socket,created,strlen(created));
            write(socket,contentLength,strlen(contentLength));
        } else {
            int file = open(resName,O_RDWR | O_TRUNC);
            char* newBuff = (char*)malloc(32768);
            if(contentLen > 0) {
                while(contentLen > 0) {
                    int clientFile = read(socket,newBuff,32768);
                    write(file, newBuff, clientFile);
                    contentLen -= clientFile;
                }
            } else {
                bool readComplete = false;
                while(!readComplete) {
                    int clientFile = read(socket,newBuff,32768);
                    if(clientFile == 0) {
                        readComplete = true;
                    }
                    write(file, newBuff, clientFile);
                }
            }
            char* ok = (char*)"HTTP/1.1 200 OK\r\n";
            char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
            write(socket,ok,strlen(ok));                
            write(socket,contentLength,strlen(contentLength));
            free(newBuff);
        }
    } else {
        char* badReq = (char*)"HTTP/1.1 400 BAD REQUEST\r\n";
        char* contentLength = (char*)"Content-Length: 0\r\n\r\n";    
        write(socket,badReq,strlen(badReq));
        write(socket,contentLength,strlen(contentLength));
    }
}

// Main handles socket creation and input parsing
int main(int argc, char *argv[]) {
    char * hostname;
    char * port;
    if(argc >= 3) {
        hostname = argv[1];
        port = argv[2];
    } else if (argc == 2) {
        hostname = argv[1];
        port = defaultPort;
    } else {
        perror("No host name supplied");
        exit(EXIT_FAILURE);
    }
    struct addrinfo *addrs, hints = {};
    int main_socket;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo(hostname, port, &hints, &addrs);
    if((main_socket = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int enable = 1;
    if(setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    if(bind(main_socket, addrs->ai_addr, addrs->ai_addrlen)<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    // N is the maximum number of "waiting" connections on the socket.
    // We suggest something like 16.
    if(listen (main_socket, 16) < 0 ) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    // Your code, starting with accept(), goes here
    while(1) {
        int new_socket;
        if((new_socket = accept(main_socket, NULL, NULL))<0) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }
        char *buffer = (char*)malloc(32768);
        read(new_socket, buffer, 32768);

        char* clPointer = strstr(buffer, "Content-Length: ");
        char putLength[] = "";
        if(clPointer != NULL) {
            clPointer+=16;
            bool done = false;
            while(!done) {
                if(isdigit(clPointer[0])) {
                    strncat(putLength,&clPointer[0],1);
                    clPointer++;
                } else {
                    done = true;
                }
            }
        }
        free(buffer);
        char* tokens = strtok(buffer," ");
        char* get = (char*)"GET";
        char* put = (char*)"PUT";
        if(strncmp(get,tokens,3) == 0) {
            tokens = strtok(NULL," ");
            if(tokens[0] == '/') {
                memmove(tokens,tokens+1, strlen(tokens));
            }
            handleGet(new_socket, tokens);
        } else if (strncmp(put,tokens,3) == 0) {
            tokens = strtok(NULL," ");
            if(tokens[0] == '/') {
                memmove(tokens,tokens+1, strlen(tokens));
            }
            handlePut(new_socket, tokens, putLength);
        } else {
            char* serverError = (char*)"HTTP/1.1 500 INTERNAL SERVER ERROR\r\n";
            char* contentLength = (char*)"Content-Length: 0\r\n\r\n";   
            write(new_socket,serverError, strlen(serverError));
            write(new_socket,contentLength,strlen(contentLength));

        }
        close(new_socket);
    }
    return 0;
}
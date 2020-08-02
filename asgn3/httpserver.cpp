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
#include <pthread.h>
#include <queue>
#define defaultPort (char*)"80"
#define magicNumber (char*)"23809324"
#define magicNumberLength 8
using namespace std;

int logFile = 0;
int aliasFile = 0;
bool logRequested = false;
int logOffset = 0;

queue <int> requestQueue;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t offsetMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t aliasMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

// Function to write data to log files
void logDataWriter(int file, int dataOffset) {
    char hexDigits[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    bool readComplete = false;
    char* buffer = (char*)malloc(20);
    int charCount = 0;
    while(!readComplete) {
        int readFile = read(file,buffer,20);
        if(readFile == 0) {
            readComplete=true;
        } else {
            char row[8]; //first 8 characters of row
            char data[60]; // 60 bytes of data
            charCount += 20;
            sprintf(row,"%08d",charCount);
            pwrite(logFile,row,strlen(row),dataOffset);
            dataOffset+=8;
            for(int i = 0 ; i<readFile ; i++) {
                char byte[3];
                //sprintf(byte," %x",buffer[i]);
                byte[0] = ' ';
                int firstChar = buffer[i]/16;
                int secondChar = buffer[i]%16;
                byte[1] = hexDigits[firstChar];
                byte[2] = hexDigits[secondChar];
                strncat(data,byte,3);
            }
            pwrite(logFile,data,strlen(data),dataOffset);
            dataOffset += strlen(data);
            pwrite(logFile,"\n",strlen("\n"),dataOffset);
            dataOffset++;
        }
    }
    pwrite(logFile,"========\n",strlen("========\n"),dataOffset);
}

void logPatch(char* target, char* alias, int dataOffset) {
    char hexDigits[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    char* fullString = (char*)malloc(7 + strlen(alias) + strlen(target));
    sprintf(fullString, "ALIAS %s %s", target, alias);
    int charCount = 0;
    for(int i = 0; i < (int)strlen(fullString); i += 20) {
        char row[8]; //first 8 characters of row
        char data[60]; // 60 bytes of data
        charCount += 20;
        sprintf(row,"%08d",charCount);
        pwrite(logFile,row,strlen(row),dataOffset);
        dataOffset+=8;
        int stopCon;
        if(i+20 <= (int)strlen(fullString)) {
            stopCon = i+20;
        } else {
            stopCon = (int)strlen(fullString);
        }
        for(int j = i ; j < stopCon ; j++ ){
            char byte[3];
                //sprintf(byte," %x",buffer[i]);
            byte[0] = ' ';
            int firstChar = fullString[j]/16;
            int secondChar = fullString[j]%16;
            byte[1] = hexDigits[firstChar];
            byte[2] = hexDigits[secondChar];
            strncat(data,byte,3);
        }
        pwrite(logFile,data,strlen(data),dataOffset);
        dataOffset += strlen(data);
        pwrite(logFile,"\n",strlen("\n"),dataOffset);
        dataOffset++;
    }
    pwrite(logFile,"========\n",strlen("========\n"),dataOffset);
}

// Function to write headers to log files
void logHeaderWriter(char* request, char* resName, char* length, char* response, bool success, int file, char* patchAlias) {
    char terminator[10] = {"========\n"};
    int numRows = (atoi(length)/20);
    if(atoi(length)%20 > 0) {
        numRows++;
    }
    int extraChars = numRows*9;
    int numChars = atoi(length)*3;
    if(success) {
        lseek(file,0,0); //resets pointer to beginning of file
        char* header = (char*)malloc(16384);
        sprintf(header,"%s %s length %s\n", request,resName,length);
        int myOffset = 0;
        int dataOffset = 0;
        pthread_mutex_lock(&offsetMutex);
        myOffset = logOffset;
        logOffset += strlen(header);
        dataOffset += logOffset;
        logOffset += extraChars + numChars + strlen(terminator);
        pthread_mutex_unlock(&offsetMutex);
        pwrite(logFile, header, strlen(header), myOffset);
        if(strcmp(request,(char*)"PATCH") == 0) {
            logPatch(resName,patchAlias,dataOffset);
        } else {
            logDataWriter(file, dataOffset);
        }
        lseek(file,0,0); //resets pointer to beginning of file
    } else {
        char* failure = (char*)malloc(16384);
        sprintf(failure,"FAIL: %s %s HTTP/1.1 --- response %s\n%s", request,resName,response,terminator);
        int myOffset = 0;
        pthread_mutex_lock(&offsetMutex);
        myOffset = logOffset;
        logOffset += strlen(failure);
        pthread_mutex_unlock(&offsetMutex);
        pwrite(logFile, failure, strlen(failure), myOffset);
    }
}

// djb2 hashing algorithm from http://www.cse.yorku.ca/~oz/hash.html
// modified by adding % 8000 to get a value between 0-8000
int hashVal(char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % 8000;
}

// Function to handle GET requests
void handleGet(int socket, char* name) {
    char* requestType = (char*)"GET";
    char* resName = (char*)malloc(strlen(name));
    strcpy(resName,name);
    char validName[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-_";
    if(strlen(resName) == 27 && strspn(resName,validName) == 27) {
        int file = open(resName, O_RDONLY);
        if(access(resName,0) == 0 && file == -1) {
            char* forbidden = (char*)"HTTP/1.1 403 FORBIDDEN\r\n";
            char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
            write(socket, forbidden, strlen(forbidden));
            write(socket,contentLength,strlen(contentLength));
            if(logRequested) {
                logHeaderWriter(requestType,resName,(char*)"0",(char*)"403",false,0,(char*)"");
            }
        } else {
            int length = 0;
            if(file > 0) {
                char* buffer = (char*)malloc(16384);
                bool readComplete = false;
                while(!readComplete) {
                    int readFile = read(file,buffer,16384);
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
                char logLen[12]; //max num of digits for a 32 bit integer is 12
                sprintf(logLen, "%d", length);
                write(socket,lenStr, strlen(lenStr));
                if(logRequested) {
                    logHeaderWriter(requestType,resName,logLen,(char*)"200",true,file,(char*)"");
                }
                //file = open(resName, O_RDONLY);
                lseek(file,0,0);
                char* nextBuffer = (char*)malloc(16384);
                readComplete = false;
                while(!readComplete) {
                    int readFile = read(file,nextBuffer,16384);
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
                if(logRequested) {
                    logHeaderWriter(requestType,resName,(char*)"0",(char*)"404",false,0,(char*)"");
                }
            }
        }
    } else {
            char* badReq = (char*)"HTTP/1.1 400 BAD REQUEST\r\n";
            char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
            write(socket,badReq,strlen(badReq));
            write(socket,contentLength,strlen(contentLength));
            if(logRequested){
                logHeaderWriter(requestType,resName,(char*)"0",(char*)"400",false,0,(char*)"");
            }
    }
}

// Function to handle PUT requests
void handlePut(int socket, char* name, char* length) {
    char* requestType = (char*)"PUT";
    char* resName = (char*)malloc(strlen(name));
    strcpy(resName,name);
    char validName[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-_";
    int contentLen = 0;
    if(strcmp(length,"") != 0) {
        contentLen = atoi(length);
    }
    if(strlen(resName) == 27 && strspn(resName,validName) == 27) {
        if(open(resName, O_RDONLY) == -1) {
            int file = open(resName,O_CREAT | O_RDWR, 0666);
            char* newBuff = (char*)malloc(16384);
            if(contentLen > 0) {
                while(contentLen > 0) {
                    int clientFile = read(socket,newBuff,16384);
                    write(file, newBuff, clientFile);
                    contentLen -= clientFile;
                }
            } else {
                bool readComplete = false;
                while(!readComplete) {
                    int clientFile = read(socket,newBuff,16384);
                    if(clientFile == 0) {
                        readComplete = true;
                    }
                    write(file, newBuff, clientFile);
                }
            }
            free(newBuff);
            if(logRequested) {
                logHeaderWriter(requestType,resName,length,(char*)"201",true,file,(char*)"");
            }
            char* created = (char*)"HTTP/1.1 201 CREATED\r\n";
            char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
            write(socket,created,strlen(created));
            write(socket,contentLength,strlen(contentLength));
        } else {
            int file = open(resName,O_RDWR | O_TRUNC);
            char* newBuff = (char*)malloc(16384);
            if(contentLen > 0) {
                while(contentLen > 0) {
                    int clientFile = read(socket,newBuff,16384);
                    write(file, newBuff, clientFile);
                    contentLen -= clientFile;
                }
            } else {
                bool readComplete = false;
                while(!readComplete) {
                    int clientFile = read(socket,newBuff,16384);
                    if(clientFile == 0) {
                        readComplete = true;
                    }
                    write(file, newBuff, clientFile);
                }
            }
            if(logRequested) {
                logHeaderWriter(requestType,resName,length,(char*)"200",true,file,(char*)"");
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
        if(logRequested) {
            logHeaderWriter(requestType,resName,length,(char*)"400",false,0,(char*)"");
        }
    }
}

// Function to handle PATCH requests
void handlePatch(int socket, char* length) {
    char* headerLine = (char*)malloc(16384);
    char* aliasName = (char*)malloc(126);
    char* targetName = (char*)malloc(126);
    char* requestType = (char*)"PATCH";
    read(socket, headerLine, 16384);
    headerLine = strtok(headerLine," ");
    headerLine = strtok(NULL," ");
    strcpy(targetName,headerLine);
    headerLine = strtok(NULL," \n");
    strcpy(aliasName,headerLine);
    bool isValid = true;
    for(int i = 0 ; i < (int)strlen(aliasName) ; i++) {
        if(isprint(aliasName[i]) == 0) {
            isValid = false;
        }
    }
    if(strlen(aliasName) + strlen(targetName) > 126  || !isValid) {
        if(logRequested) {
            logHeaderWriter(requestType,targetName,length,(char*)"400",false,0,(char*)"");
        }
        char* badReq = (char*)"HTTP/1.1 400 BAD REQUEST\r\n";
        char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
        write(socket,badReq,strlen(badReq));
        write(socket,contentLength,strlen(contentLength));
    } else {
        char validName[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-_";
        if(strlen(targetName) == 27 && strspn(targetName,validName) == 27) { // a valid resource
            int tryOpen = open(targetName, O_RDONLY);
            if(tryOpen <= 0) {
                if(logRequested) {
                    logHeaderWriter(requestType,targetName,length,(char*)"404",false,0,(char*)"");
                }
                char* notFound = (char*)"HTTP/1.1 404 NOT FOUND\r\n";
                char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
                write(socket,notFound,strlen(notFound));
                write(socket,contentLength,strlen(contentLength));
            } else {
                int offset = (hashVal(aliasName) * 128) + magicNumberLength; 
                pthread_mutex_lock(&aliasMutex);
                pwrite(aliasFile,aliasName,strlen(aliasName),offset);
                pwrite(aliasFile,targetName,strlen(targetName),offset+strlen(aliasName) + 1);
                pthread_mutex_unlock(&aliasMutex);
                if(logRequested) {
                    logHeaderWriter(requestType,targetName,length,(char*)"200",true,0,aliasName);
                }
                char* ok = (char*)"HTTP/1.1 200 OK\r\n";
                char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
                write(socket,ok,strlen(ok));                
                write(socket,contentLength,strlen(contentLength));
            }
        } else { // the target is not a resource (is an alias)
            int targetIndex = hashVal(targetName) * 128 + magicNumberLength;
            char* foundAlias = (char*)malloc(strlen(targetName));
            pthread_mutex_lock(&aliasMutex);
            pread(aliasFile,foundAlias,strlen(targetName),targetIndex);
            pthread_mutex_unlock(&aliasMutex);
            if(foundAlias[0] == '\0') {
                if(logRequested) {
                    logHeaderWriter(requestType,targetName,length,(char*)"404",false,0,(char*)"");
                }
                char* notFound = (char*)"HTTP/1.1 404 NOT FOUND\r\n";
                char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
                write(socket,notFound,strlen(notFound));
                write(socket,contentLength,strlen(contentLength));
            } else {
                int offset = (hashVal(aliasName) * 128) + magicNumberLength; 
                pthread_mutex_lock(&aliasMutex);
                pwrite(aliasFile,aliasName,strlen(aliasName),offset);
                pwrite(aliasFile,targetName,strlen(targetName),offset+strlen(aliasName) + 1);
                pthread_mutex_unlock(&aliasMutex);
                if(logRequested) {
                    logHeaderWriter(requestType,targetName,length,(char*)"200",true,0,aliasName);
                }
                char* ok = (char*)"HTTP/1.1 200 OK\r\n";
                char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
                write(socket,ok,strlen(ok));                
                write(socket,contentLength,strlen(contentLength));
            }
        }   
    }
}

void findResource(int socket, char* name, char* length, char* request) {
    char* get = (char*)"GET";
    char* put = (char*)"PUT";
    char validName[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-_";
    if(strlen(name) == 27 && strspn(name,validName) == 27) {
        if(strncmp(request,get,3) == 0) {
            handleGet(socket,name);
        } else if(strncmp(request,put,3) == 0) {
            handlePut(socket,name,length);
        }
    } else {
        int aliasIndex = hashVal(name) * 128 + magicNumberLength;
        char* foundAlias = (char*)malloc(strlen(name));
        pthread_mutex_lock(&aliasMutex);
        pread(aliasFile,foundAlias,strlen(name),aliasIndex);
        pthread_mutex_unlock(&aliasMutex);
        if(foundAlias[0] == '\0') {
            char* notFound = (char*)"HTTP/1.1 404 NOT FOUND\r\n";
            char* contentLength = (char*)"Content-Length: 0\r\n\r\n";
            write(socket,notFound,strlen(notFound));
            write(socket,contentLength,strlen(contentLength));
        } else {
            char* targetName = (char*)malloc(126);
            pthread_mutex_lock(&aliasMutex);
            pread(aliasFile, targetName,126,aliasIndex+strlen(name) + 1);
            pthread_mutex_unlock(&aliasMutex);
            findResource(socket,targetName,length,request);
        }
    }
}

// Function to handle header parsing 
void newConnection (int new_socket) {
    char *buffer = (char*)malloc(16384);
    read(new_socket, buffer, 16384);
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
    char* tokens = strtok(buffer," ");
    char* get = (char*)"GET";
    char* put = (char*)"PUT";
    char* patch = (char*)"PATCH";
    if(strncmp(patch,tokens,5) == 0) {
        handlePatch(new_socket,putLength);
    } else if(strncmp(get,tokens,3) == 0) {
        tokens = strtok(NULL," ");
        if(tokens[0] == '/') {
            memmove(tokens,tokens+1, strlen(tokens));
        }
        //handleGet(new_socket, tokens);
        findResource(new_socket, tokens, 0, get);
    } else if (strncmp(put,tokens,3) == 0) {
        tokens = strtok(NULL," ");
        if(tokens[0] == '/') {
            memmove(tokens,tokens+1, strlen(tokens));
        }
        //handlePut(new_socket, tokens, putLength);
        findResource(new_socket, tokens, putLength, put);
    } else {
        char* serverError = (char*)"HTTP/1.1 500 INTERNAL SERVER ERROR\r\n";
        char* contentLength = (char*)"Content-Length: 0\r\n\r\n";   
        write(new_socket,serverError, strlen(serverError));
        write(new_socket,contentLength,strlen(contentLength));
    }
}

// Function to handle threads
void* threadCreator(void * args) {
    (void)args; 
    //void* argument is required for pthread_create but no parameters are passed
    //fixes warning for unused parameter
    while(1) {
        int socket = 0;
        pthread_mutex_lock(&mutex);
        if(requestQueue.empty()) {
            pthread_cond_wait(&condition,&mutex);
        }
        if(!requestQueue.empty()) {
            socket = requestQueue.front();
            requestQueue.pop();
        }
        pthread_mutex_unlock(&mutex);
        newConnection(socket);
        close(socket);
    }
    return NULL;
}

// Main handles socket creation and input parsing
int main(int argc, char *argv[]) {
    char * hostname;
    char * port;
    int numThreads = 4;
    bool hasAlias = false;
    if(argc >= 3) {
        for(int i = 1 ; i < argc ; i++) {
            char* threadFlag  = (char*)"-N";
            char* logFlag = (char*)"-l";
            char* aliasFlag = (char*)"-a";
            if(strcmp(argv[i],threadFlag) == 0) {
                numThreads = atoi(argv[i+1]);
                i++;
            } else if(strcmp(argv[i], logFlag) == 0) {
                logFile = open(argv[i+1],O_CREAT | O_RDWR| O_TRUNC, 0666);
                logRequested = true;
                i++;
            } else if(strcmp(argv[i], aliasFlag) == 0) {
                aliasFile = open(argv[i+1],O_RDWR);
                if(aliasFile <= 0) {
                    aliasFile = open(argv[i+1],O_CREAT | O_RDWR| O_TRUNC, 0666);
                    pthread_mutex_lock(&aliasMutex);
                    pwrite(aliasFile,magicNumber,magicNumberLength,0);
                    pthread_mutex_unlock(&aliasMutex);
                } else {
                    char* firstLineCheck = (char*)malloc(magicNumberLength);
                    char* magicNumberVar = magicNumber;
                    pthread_mutex_lock(&aliasMutex);
                    pread(aliasFile,firstLineCheck,magicNumberLength,0);
                    pthread_mutex_unlock(&aliasMutex);
                    if(strcmp(magicNumberVar,firstLineCheck) != 0) {
                        perror("Invalid mapping file");
                        exit(EXIT_FAILURE);
                    }
                    free(firstLineCheck);
                    firstLineCheck = NULL;
                }
                hasAlias = true;
                i++;
            } else {
                hostname = argv[i];
                port = argv[i+1];
                i++;
            }
        }
    } else if (argc == 2) {
        hostname = argv[1];
        port = defaultPort;
    } else {
        perror("No host name supplied");
        exit(EXIT_FAILURE);
    }
    if(!hasAlias) {
        perror("No mapping file specified");
        exit(EXIT_FAILURE);
    }
    pthread_t *threads = (pthread_t*)malloc(sizeof(pthread_t)*numThreads);
    // create all threads
    for(int i = 0 ; i < numThreads ; i++) {
        pthread_create(&threads[i],NULL,threadCreator,NULL);
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
        pthread_mutex_lock(&mutex);
        requestQueue.push(new_socket);
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&mutex);
    }
    return 0;
}
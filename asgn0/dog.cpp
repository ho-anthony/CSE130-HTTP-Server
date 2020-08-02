// Author: Anthony Ho
// CruzID: Ankho
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <stdio.h>

// Prints from STDIN till EOF
void takeStdin() {
    char *buff = (char*)malloc(32768);
    int readFile = read(STDIN_FILENO, buff, 32768);
    while(readFile != 0) {
       write(1,buff,readFile);
       readFile = read(STDIN_FILENO, buff, 32768);
    }
    free(buff); // frees the buffer
 }

// Prints from file
void printFile(char *file) {
    int inFile = open(file, O_RDONLY);
    char *buff = (char*)malloc(32768);
    bool readComplete = false;
    while(!readComplete) { // loop to handle files larger than 32KiB
        int readFile = read(inFile,buff,32768);
        if (readFile == 0) {
            readComplete = true;
        }
        if(readFile == -1 ) { //If read is negative at this point, the file is a directory
            warn("%s", file);
            readComplete = true;
        }
        write(1,buff,readFile);
    }
    free(buff); // frees the buffer
    close(inFile); // closes the file
}

////// MAIN //////
int main(int argc, char *argv[]) {
    if(argc == 1) { // checks if there are no arguments
        takeStdin();
    }
    for(int i = 1 ; i<argc ; i++) {
        if(open(argv[i], O_RDONLY ) > 0) { // checks if the argument is a file
            printFile(argv[i]);
        } else if (argv[i][0] == '-') { // checks if the argument is a dash
            takeStdin();
        } else {
            warn("%s", argv[i]); // the argument is not a file, dash, or directory
        }
    
    }
    return 0;
}
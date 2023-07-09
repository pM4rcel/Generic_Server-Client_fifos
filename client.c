#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PAGE_SIZE 4096

const char* SERVER_RESPONSES = "./out/responses";
const char* CLIENT_REQUESTS = "./out/requests";

int main(){
    int requests, responses;
    char buffer[PAGE_SIZE] = {};
    size_t responseLength;

    if((requests = open(CLIENT_REQUESTS, O_WRONLY)) == -1){
        perror("[Server]");
        exit(EXIT_FAILURE);
    }

    if((responses = open(SERVER_RESPONSES, O_RDONLY)) == -1){
        perror("[Server]");
        exit(EXIT_FAILURE);
    }


    printf("Connected to server succesfully!\n");

    do{
        fgets(buffer, PAGE_SIZE, stdin);
        buffer[strlen(buffer) - 1] = '\0'; 

        write(requests, buffer, PAGE_SIZE);
        read(responses, &responseLength, sizeof(size_t));
        read(responses, buffer, PAGE_SIZE);
        printf("%s\n", buffer);
        if(!strcmp(buffer, "Exit successfully!"))
            break;
    }while(1);

    close(requests);
    close(responses);
}
#include <fcntl.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#include "command.h"

#define HELP_MESSAGE "quit - quit the application\nlogin : <username> - to login to the server\nlogout - to logout as the current user\nget-logged-users - displays information (username, hostname for remote login, time entry was made) about all users that are logged in the operating system\nget-proc-info : <pid> - displays information (name, state, ppid, uid, vmsize) about the process whose pid is specified"
#define PAGE_SIZE 4096
#define LINE_SIZE 64

const char* SERVER_RESPONSES = "./out/responses";
const char* CLIENT_REQUESTS = "./out/requests";
const char* USERS_DB = "./out/users.txt";
const char* processInfo[] = {
    "Name",
    "State",
    "PPid",
    "Uid",
    "VmSize"
};

const size_t numOfInfoProcess = sizeof(processInfo) / sizeof(void*);

size_t isLogged;

const Command commands[] = {
    {(char* []){"quit"}, 1},
    {(char* []){"help"}, 1},
    {(char* []){"login", ":"}, 3},
    {(char* []){"logout"}, 1},
    {(char* []){"get-logged-users"}, 1},
    {(char* []){"get-proc-info", ":"}, 3}
};

const size_t numOfCommands = sizeof(commands) / sizeof(Command);

ssize_t getCommand(Command* command){
    for(size_t index = 0; index < numOfCommands; index++)
        if(command->size == commands[index].size && !strcmp(commands[index].tokens[0], command->tokens[0]))
            return index;

    return -1;
}

// WARNING: if the last username in the file is not followed by a newline
//          it will fail to login
size_t login_user(char* username){
    char line[LINE_SIZE] = {};
    FILE* usersFile = fopen(USERS_DB, "r");

    while(fgets(line, LINE_SIZE, usersFile)){
        line[strlen(line) - 1] = '\0';
        if(!strcmp(line, username))
            return ++isLogged;
    }

    fclose(usersFile);
    return 0;
}

char* get_logged_users(char* buffer){
    memset(buffer, 0, PAGE_SIZE);
    struct utmpx* userInfo;
    char user[__UT_NAMESIZE];
    char host[__UT_HOSTSIZE];
    char line[PAGE_SIZE];
    size_t countUser = 0;
    setutxent();
    while((userInfo = getutxent())){
        sprintf(line, "%zu\n", ++countUser);
        strcat(buffer, line);

        strncpy(user, userInfo->ut_user, __UT_NAMESIZE);
        user[__UT_NAMESIZE - 1] = '\0';
        sprintf(line, "User: %s\n", user);
        strcat(buffer, line);

        strncpy(host, userInfo->ut_user, __UT_NAMESIZE);
        host[__UT_HOSTSIZE - 1] = '\0';
        sprintf(line, "Hostname: %s\n", host);
        strcat(buffer, line);

        strcat(buffer, ctime((const time_t*)userInfo->ut_tv.tv_sec));
    }
    endutxent();
    return buffer;
}

char* get_proc_info(__pid_t processID, char* buffer){
    char path[PATH_MAX] = {};
    char line[LINE_SIZE * 2] = {};
    memset(buffer, 0, PAGE_SIZE);

    sprintf(path, "/proc/%i/status", processID);

    if(access(path, F_OK) == -1){
        return "Invalid PID!";
    }

    FILE* processFile = fopen(path, "r");

    size_t messageLength = 0;
    while(fgets(line, LINE_SIZE * 2, processFile))
        for(size_t index = 0; index < numOfInfoProcess; ++index)
            if(strstr(line, processInfo[index])){
                strcat(buffer, line);
                messageLength += strlen(line);
            }
    buffer[messageLength - 1] = '\0';
    
    return buffer;
}

char* handleRequest(char* message){
    int anonymousChannel[2];

    if(pipe(anonymousChannel) == -1){
        perror("[Server] pipe()");
        return "Server error!";
    }

    Command* parsedCommand = parse(message);
    ssize_t commandIndex = getCommand(parsedCommand);


    if(commandIndex == -1){
        erase(parsedCommand);
        return "Invalid command!";
    }

// The commands that can be executed any time, despite the client status (if it is logged in or not)
    if(commandIndex == 0){
        isLogged = 0;
        erase(parsedCommand);
        return "Exit successfully!";
    }

    if(commandIndex == 1){
        return HELP_MESSAGE;
    }

    __pid_t forking = fork();
    if(forking == -1){
        perror("[Server] fork() in child");
        return "Server error!";
    }else if(forking == 0){
        close(anonymousChannel[0]);
        write(anonymousChannel[1], &isLogged, sizeof(size_t));
        close(anonymousChannel[1]);
        exit(EXIT_SUCCESS);
    }else
        close(anonymousChannel[1]);


    size_t status;
    read(anonymousChannel[0], &status, sizeof(size_t));

    close(anonymousChannel[0]);

    if(commandIndex == 2){
        if(status == 1){
            erase(parsedCommand);
            return "You're already logged in!";
        }
        size_t loginStatus = login_user(parsedCommand->tokens[2]);
        erase(parsedCommand);
        return loginStatus == 1 ? "Logged In succesfully!" : "The user does not exist!";
    }

    if(status == 1)
        switch (commandIndex){
            case 3:
                isLogged = 0;
                erase(parsedCommand);
                return "Logged out successfully!";
            case 5:
                return get_proc_info(atoi(parsedCommand->tokens[2]), message);
            case 4:
                return get_logged_users(message);
        }
    erase(parsedCommand);
    return "You're not logged in!";
}

void init(){

    if(mkfifo(SERVER_RESPONSES, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) == -1){
        if(errno != EEXIST){
            perror("[Server] mkfifo() responses");
            exit(EXIT_FAILURE);
        }

        printf("[Server] Warning: %s fifo already exists\n", SERVER_RESPONSES);
    }

    if(mkfifo(CLIENT_REQUESTS, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) == -1){
        if(errno != EEXIST){
            perror("[Server] mkfifo() requests");
            exit(EXIT_FAILURE);
        }

        printf("[Server] Warning: %s fifo already exists\n", CLIENT_REQUESTS);            
    }
}

int main(){
    int requests, responses;
    int sockets[2];
    char buffer[PAGE_SIZE] = {};
    size_t messageLength;

    init();

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == -1){
        perror("[Server] socketpair()");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Started successfully...\n");

    if((requests = open(CLIENT_REQUESTS, O_RDONLY)) == -1){
        perror("[Server] open() requests");
        exit(EXIT_FAILURE);
    }

    if((responses = open(SERVER_RESPONSES, O_WRONLY)) == -1){
        perror("[Server] open() responses");
        exit(EXIT_FAILURE);
    }

    __pid_t forking = fork();

    if(forking == -1){
        perror("[Server] fork() main");
        exit(EXIT_FAILURE);
    }else if(forking == 0)
        close(sockets[1]);
    else
        close(sockets[0]);

    do{
        if(forking == 0){
            read(sockets[0], buffer, PAGE_SIZE);
            write(sockets[0], handleRequest(buffer), PAGE_SIZE);
            continue;
        }

        read(requests, buffer, PAGE_SIZE);

        write(sockets[1], buffer, PAGE_SIZE);
        read(sockets[1], buffer, PAGE_SIZE);
        
        write(responses, &messageLength, sizeof(size_t));
        write(responses, buffer, PAGE_SIZE);
    }while(1);

    close(requests);
    close(responses);
}
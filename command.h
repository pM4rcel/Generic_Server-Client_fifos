#include <stdlib.h>
#include <string.h>

#define COMMAND_SEPARATORS " "
#define MAX_COMMAND_TOKENS 3

typedef struct{
    char** tokens;
    size_t size;
} Command;

// maps the string to an Command
// if the string has more than 3 tokens, then the rest of them are ignored
Command* parse(char* str){
    size_t numOfTokens = 0;
    Command* parsing = (Command*)malloc(sizeof(Command));
    memset(parsing, 0, sizeof(Command));
    parsing->tokens = (char**)malloc(MAX_COMMAND_TOKENS * sizeof(char*));

    char* token = strtok(str, COMMAND_SEPARATORS);

    while(token && numOfTokens < 3){
        parsing->tokens[numOfTokens++] = strdup(token);
        token = strtok(NULL, COMMAND_SEPARATORS);
    }

    parsing->size = numOfTokens;

    return parsing;
}

// dealocate the space of a command
void erase(Command* com){
    for(size_t index = 0; index < com->size; index++)
        free(com->tokens[index]);
    free(com);
}
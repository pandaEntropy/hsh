#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSH_RD_BUFSIZE 1024
#define MSH_TOK_BUFSIZE 64
#define MSH_TOK_DELIM [" \t\r\n\a"]

void msh_loop();
char *msh_read_line();
char **msh_split_line(char *line);

int main(int argc, char **argv) {
    
    msh_loop();
    
    return EXIT_SUCCESS;
}

void msh_loop(){

    char *line;
    char **args;
    int status;
    
    do{
        printf("> ");
        line = msh_read_line();
        args = msh_split_line(line);
        status = msh_execute_line(args);
        
        free(line);
        free(args);
    } while (status);
}


char *msh_read_line(){

    int bufsize = MSH_RD_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if(!buffer){
        fprintf(stderr, "msh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while(1){

        c = getchar();
        
        // Replace the last character with null if we hit EOF
        if(c == EOF || c == '\n'){
            buffer[position] = '\0';
            return buffer;
        }
        else{
            buffer[position] = c;
        }
        position++;

        if(position >= bufsize){
            bufsize += MSH_RD_BUFSIZE;
            char *temp = realloc(buffer, bufsize);
            if(!temp){
                fprintf(stderr, "msh: allocation error\n");
                exit(EXIT_FAILURE);
            }
            buffer = temp;
        }
    }  
}



char **msh_split_line(char *line){
    
    int bufsize = MSH_TOK_BUFSIZE;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if(!tokens){
        fprintf("msh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, MSH_TOK_DELIM);

    while(token != NULL){
        tokens[position] = token;
        position++;

        if(position >= bufsize){
            bufsize = MSH_TOK_BUFSIZE;
            char *temp = realloc(tokens, bufsize * sizeof(char*));
            if(!temp){
                fprintf(stderr, "msh: allocation error\n");
                exit(EXIT_FAILURE);
            }
            tokens = temp;
        }

        token = strtok(NULL, MSH_TOK_DELIM);
    }

    tokens[position] = NULL;
    return tokens;

}
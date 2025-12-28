#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MSH_RD_BUFSIZE 1024
#define MSH_TOK_BUFSIZE 64
#define MSH_TOK_DELIM " \t\r\n\a"

void msh_loop();
char *msh_read_line();
char **msh_split_line(char *line);
int msh_cd(char **args);
int msh_help(char **args);
int msh_exit(char **args);
int msh_execute_line(char **args);

char *builtin_str[] = {"cd", "help", "exit"};
int (*builtin_func[]) (char **) = {msh_cd,  msh_help, msh_exit};

int main(){
    
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

    int bufsize = MSH_RD_BUFSIZE; //Amount of characters
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
    
    int bufsize = MSH_TOK_BUFSIZE; //Amount of arguments
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if(!tokens){
        fprintf(stderr, "msh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, MSH_TOK_DELIM);

    while(token != NULL){
        tokens[position] = token;
        position++;

        if(position >= bufsize){
            bufsize = MSH_TOK_BUFSIZE;
            char **temp = realloc(tokens, bufsize * sizeof(char*));
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

int msh_launch(char **args){

    pid_t pid, wpid;
    int status;

    pid = fork();
    if(pid == 0){
        //Child process
        if(execvp(args[0], args) == -1){
            perror("msh");
        }
        exit(EXIT_FAILURE);
    }
    else if(pid < 0){
        //Error forking
        perror("msh");    
    }
    else{
        //Parent process
        wpid = waitpid(pid, &status, 0); //Sleep until the childs exits
    }
    
    return 1;
    
}

int msh_num_builtins(){
    return sizeof(builtin_str) / sizeof(char*);
}

int msh_cd(char **args){
    if(args[0] == NULL){
        fprintf(stderr, "msh: expected argument to \"cd\"\n");
    }
    else{
        if(chdir(args[1]) != 0){
            perror("msh");
        }
    }
    return 1;
}

int msh_help(char **args){
    printf("This is my first shell: MSH\n");
    printf("Type commands and hit enter.\n");
    printf("The following commands are built in:\n");

    for(int i = 0; i < msh_num_builtins(); i++){
        printf(" %s\n", builtin_str[i]);
    }

    return 1;
}

int msh_exit(char **args){
    return 0;
}

int msh_execute_line(char **args){
    if(args[0] == NULL){
        return 1;
    }

    for(int i = 0; i < msh_num_builtins(); i++){
        if(strcmp(args[0], builtin_str[i]) == 0){
            return builtin_func[i](args);
        }
    }

    return msh_launch(args);
}
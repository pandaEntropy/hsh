#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define HSH_RD_BUFSIZE 1024
#define HSH_TOK_BUFSIZE 64
#define HSH_TOK_DELIM " \t\r\n\a"

#define HSH_BLOCK_BUFSIZE 32
#define HSH_BLOCK_DELIM "|>&"

typedef enum Operator{
    OP_PIPE,
    OP_REDIR,
    OP_AND,
    OP_OR,
    OP_NONE
}Operator;

void hsh_loop();
char *hsh_read_line();
char **hsh_split_line(char *line);
int hsh_cd(char **args);
int hsh_help(char **args);
int hsh_exit(char **args);
int hsh_execute_line(char **args, Operator op, int opcount);
int hsh_num_builtins();
int hsh_launch(char **args);
Operator parse_ops(char *line, int *opcount);
char **parse_block(char *block);
int handle_pipe(char **blocks, int pipecount);
int handle_redir(char **args, char **out);
int hsh_execvp(char *name, char **args);
int handle_and(char **args1, char **args2);
int handle_or(char **args1, char **args2);

char *builtin_str[] = {"cd", "help", "exit"};
int (*builtin_func[]) (char **) = {hsh_cd,  hsh_help, hsh_exit};

int main(){

    hsh_loop();

    return EXIT_SUCCESS;
}

void hsh_loop(){

    char *line;
    int status, opcount;
    Operator op;
    char **blocks;

    do{
        printf("> ");
        line = hsh_read_line();
        op = parse_ops(line, &opcount);
        blocks = hsh_split_line(line);
        status = hsh_execute_line(blocks, op, opcount);

        free(line);
        free(blocks);
    }while(status != -1);
}


char *hsh_read_line(){

    int bufsize = HSH_RD_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if(!buffer){
        fprintf(stderr, "hsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while(1){
        c = getchar();

        if(c == EOF || c == '\n'){
            buffer[position] = '\0';
            return buffer;
        }
        else{
            buffer[position] = c;
        }
        position++;

        if(position >= bufsize){
            bufsize += HSH_RD_BUFSIZE;
            char *temp = realloc(buffer, bufsize);
            if(!temp){
                fprintf(stderr, "hsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
            buffer = temp;
        }
    }
}

char **hsh_split_line(char *line){

    int bufsize = HSH_BLOCK_BUFSIZE;
    int position = 0;
    char **blocks = malloc(bufsize * sizeof(char*));
    char *block;

    if(!blocks){
        fprintf(stderr, "hsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    char *dup_line = strdup(line);

    block = strtok(dup_line, HSH_BLOCK_DELIM);

    while(block != NULL){
        blocks[position] = block;
        position++;

        if(position >= bufsize){
            bufsize += HSH_BLOCK_BUFSIZE;
            char **temp = realloc(blocks, bufsize * sizeof(char*));
            if(!temp){
                fprintf(stderr, "hsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
            blocks = temp;
        }

        block = strtok(NULL, HSH_BLOCK_DELIM);
    }

    blocks[position] = NULL;
    return blocks;
}

int hsh_execute_line(char **blocks, Operator op, int opcount){
    char **args1 = parse_block(blocks[0]);
    char **args2 = parse_block(blocks[1]);

    switch(op){
        case OP_PIPE:
            return handle_pipe(blocks, opcount);

        case OP_REDIR:
            return handle_redir(args1, args2);

        case OP_AND:
            return handle_and(args1, args2);

        case OP_OR:
            return handle_or(args1, args2);

        case OP_NONE:
            return hsh_launch(args1);
    }

    fprintf(stderr, "hsh: operator not found\n");
    return 1;
}

int hsh_launch(char **args){
    if(args == NULL) return 1;

    for(int i = 0; i < hsh_num_builtins(); i++){
        if(strcmp(args[0], builtin_str[i]) == 0){
            return builtin_func[i](args);
        }
    }

    pid_t pid;
    int status;

    pid = fork();
    if(pid == 0){
        execvp(args[0], args);
        perror("hsh: command execution failed");
        exit(EXIT_FAILURE);
    }
    else if(pid < 0){
        perror("hsh: fork failed");
        return 1;
    }
    else{
        waitpid(pid, &status, 0);
    }

    if(WIFEXITED(status)){
        return WEXITSTATUS(status);
    }

    return 1;
}

int hsh_num_builtins(){
    return sizeof(builtin_str) / sizeof(char*);
}

int hsh_cd(char **args){
    if(args[0] == NULL){
        fprintf(stderr, "hsh: expected argument to \"cd\"\n");
    }
    else{
        if(chdir(args[1]) != 0){
            perror("hsh");
            return 1;
        }
    }
    return 0;
}

int hsh_help(char **args){
    printf("This is the hollow shell.\n");
    printf("The following commands are built in:\n");

    for(int i = 0; i < hsh_num_builtins(); i++){
        printf(" %s\n", builtin_str[i]);
    }

    return 0;
}

int hsh_exit(char **args){
    return -1;
}

Operator parse_ops(char *line, int *opcount){
    int count = 0;
    Operator op = OP_NONE;
    for(int i = 0; line[i] != '\0'; i++){
        if(line[i] == '|'){

            if(line[i + 1] && line[i + 1] == '|'){
                if(op != OP_NONE){
                    fprintf(stderr, "hsh: Invalid operator sequence\n");
                    return -1;
                }
                op = OP_OR;
                count++;
                i++;
                continue;
            }
            op = OP_PIPE;
            count++;

        }

        if(line[i] == '>'){
            if(op != OP_NONE){
                fprintf(stderr, "hsh: Invalid operator sequence\n");
                return -1;
            }
            op = OP_REDIR;
            
        }

        if(line[i] == '&'){
            if(line[i + 1] && line[i + 1] == '&'){
                if(op != OP_NONE){
                    fprintf(stderr, "hsh: Invalid operator sequence\n");
                    return -1;
                }
                op = OP_AND;
                count++;
                i++;
            }
        }
    }

    *opcount = count;
    return op;
}

char **parse_block(char *block){
    if(block == NULL) return NULL;

    char *dup_block = strdup(block);

    int bufsize = HSH_TOK_BUFSIZE;
    char **tokens = malloc(bufsize * sizeof(char*));
    int index = 0;

    char *token = strtok(dup_block, HSH_TOK_DELIM);
    while(token != NULL){
        tokens[index] = strdup(token);
        index++;

        if(index >= HSH_TOK_BUFSIZE){
            bufsize += bufsize;
            char **temp = realloc(tokens, bufsize * sizeof(char*));

            if(!temp){
                fprintf(stderr, "hsh: allocation failed\n");
                exit(EXIT_FAILURE);
            }

            tokens = temp;
        }

        token = strtok(NULL, HSH_TOK_DELIM);
    }

    tokens[index] = NULL;
    free(dup_block);
    return tokens;
}

int handle_pipe(char **blocks, int pipecount){
    if(blocks == NULL) return 1;

    int fd[2];
    int in_fd = STDIN_FILENO;
    int num_commands = pipecount + 1;

    for(int i = 0; i < num_commands; i++){
        if(blocks[i] == NULL || blocks[i][0] == '\0'){
            fprintf(stderr, "hsh: Syntax error\n");
            return 1;
        }
    }

    for(int i = 0; i < num_commands; i++){
        if(num_commands - 1 > i){
            if(pipe(fd) != 0){
                perror("hsh: pipe failed");
                return 1;
            }
        }

        pid_t pid = fork();
        if(pid == 0){
            if(in_fd != STDIN_FILENO){
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            if(num_commands - 1 > i){
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
                close(fd[0]);
            }

            char **args = parse_block(blocks[i]);
            int exec_status = hsh_execvp(args[0], args);
            if(exec_status != 0){
                perror("hsh: command execution failed");
            }
            exit(exec_status);
        }
        else if(pid < 0){
            perror("hsh: fork failed");
            return 1;
        }
        else{
            if(in_fd != STDIN_FILENO){
                close(in_fd);
            }

            if(num_commands - 1 > i){
                in_fd = fd[0];
                close(fd[1]);
            }
        }
    }

    int status;
    pid_t wpid = wait(&status);
    int exit_code = 0;
    while(wpid != -1){
        if(WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0)){
            exit_code = 1;
        }

        wpid = wait(&status);
    }

    return exit_code;
}

int handle_redir(char **args, char **out){
    if(!args || !out) return 1;

    pid_t pid = fork();

    if(pid == 0){
        int fd = open(*out, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if(fd < 0){
            perror("hsh: failed to open file");
            exit(EXIT_FAILURE);
        }

        dup2(fd, STDOUT_FILENO);
        close(fd);

        int exec_status = hsh_execvp(args[0], args);
        if(exec_status != 0){
            perror("hsh: command execution failed fff");
        }
        exit(exec_status);
    }
    else if(pid < 0){
        perror("hsh: fork failed");
        return 1;
    }

    int status;
    waitpid(pid, &status, 0);

    // Child terminated normally
    if(WIFEXITED(status)){
        return WEXITSTATUS(status);
    }

    //child terminated by a signal
    return 1;
}

int hsh_execvp(char *name, char **args){
    if(name == NULL || args == NULL){
        return 1;
    }

    for(int i = 0; i < hsh_num_builtins(); i++){
        if(strcmp(name, builtin_str[i]) == 0){
            return builtin_func[i](args);
        }
    }

    execvp(name, args);
    return 1;
}

int handle_and(char **args1, char **args2){
    if(args1 == NULL || args2 == NULL) return 1;

    int status = hsh_launch(args1);

    if(status == 0){
        status = hsh_launch(args2);
    }

    return status;
}

int handle_or(char **args1, char **args2){
    if(args1 == NULL || args2 == NULL) return 1;

    int status = hsh_launch(args1);

    if(status != 0){
        status = hsh_launch(args2);
    }

    return status;
}

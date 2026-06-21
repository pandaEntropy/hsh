#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libgen.h>

#define MAX_USTACK 40

typedef enum Cmdtype{
    CMD_MOVE,
    CMD_DELETE,
    CMD_INTERNAL,
    CMD_CREATE_FILE,
    CMD_CREATE_DIR,
    CMD_NOUNDO
}Cmdtype;

typedef enum MemType{
    MEM_CREATE,
    MEM_MOVE,
    MEM_DELETE,
    MEM_INTERNAL
}MemType;

typedef struct CmdRule{
    char *name;
    Cmdtype type;
}CmdRule;

typedef struct CreateMemento{
    char **paths;
    char **parent_dirs;

    bool is_dir;

    struct timeval (*times)[2];
}CreateMemento;

typedef struct MoveMemento{
    char *source;
    char *dest;

    char *source_pdir;
    char *dest_pdir;

    struct timeval spdir_time;
    struct timeval dpdir_time;
}MoveMemento;

typedef struct DeleteMemento{
    char **args;
}DeleteMemento;

typedef struct InternalMemento{
    char *cwd;
}InternalMemento;

typedef struct HollowMemento{
    MemType memtype;
    union{
        CreateMemento *creatmem;
        MoveMemento *movmem;
        DeleteMemento *delmem;
        InternalMemento *intmem;
    }mem;
}HollowMemento;

void upush(HollowMemento holmem);
void get_timestamps(char *name, struct timeval *time);

static HollowMemento ustack[MAX_USTACK];
static int utop = -1;

CmdRule rule_table[] = {
    {"mv", CMD_MOVE}, {"rename", CMD_MOVE}, {"cd", CMD_INTERNAL},
    {"rm", CMD_DELETE}, {"rmdir", CMD_DELETE}, {"unlink", CMD_DELETE},
    {"mkdir", CMD_CREATE_DIR}, {"touch", CMD_CREATE_FILE}, {"ln", CMD_CREATE_FILE}
};

Cmdtype get_cmdtype(char *name){
    int length = sizeof(rule_table) / sizeof(CmdRule); 
    for(int i = 0; i < length; i++){
        if(strcmp(name, rule_table[i].name) == 0) 
            return rule_table[i].type;
    }

    return CMD_NOUNDO;
}

/*
 * TODO later implement inode tracking
 * Add error checks
 * Change to the arena with a pool at the end method
*/

void create_creatmem(char **args, int arg_idx, int argc, Cmdtype type){
    int num_paths = argc - arg_idx;

    CreateMemento *creatmem = malloc(sizeof(CreateMemento));

    creatmem->paths = malloc((num_paths + 1) * sizeof(char *));
    creatmem->parent_dirs = malloc(num_paths * sizeof(char *));
    creatmem->times = malloc(sizeof(struct timeval) * num_paths * 2);
    creatmem->is_dir = (type == CMD_CREATE_DIR);

    for(int i = 0; args[i + arg_idx] != NULL; i++){
        creatmem->paths[i] = strdup(args[i + arg_idx]);

        char tmp_path[PATH_MAX];
        strcpy(tmp_path, args[i + arg_idx]);

        char *dir = dirname(tmp_path);
        creatmem->parent_dirs[i] = strdup(dir);

        get_timestamps(dir, creatmem->times[i]);
    }

    creatmem->paths[num_paths] = NULL;

    HollowMemento holmem;
    holmem.memtype = MEM_CREATE;
    holmem.mem.creatmem = creatmem;

    upush(holmem);
}

void create_movmem(char **args, int arg_idx, int argc){
    MoveMemento *movmem = malloc(sizeof(MoveMemento));
    movmem->source = strdup(args[arg_idx]);
    movmem->dest = strdup(args[arg_idx + 1]);

    char tmp_path[PATH_MAX];

    snprintf(tmp_path, sizeof(tmp_path), "%s", args[arg_idx]);
    char *dir = dirname(tmp_path);
    movmem->source_pdir = strdup(dir);

    struct timeval spdir_time;
    get_timestamps(dir, &spdir_time);
    movmem->spdir_time = spdir_time;

    snprintf(tmp_path, sizeof(tmp_path), "%s", args[arg_idx + 1]);
    dir = dirname(tmp_path);
    movmem->dest_pdir = strdup(dir);

    struct timeval dpdir_time;
    get_timestamps(dir, &dpdir_time);
    movmem->dpdir_time = dpdir_time;

    HollowMemento holmem;
    holmem.memtype = MEM_MOVE;
    holmem.mem.movmem = movmem;

    upush(holmem);
}

void create_delmem(char **args, int arg_idx, int argc){
    DeleteMemento *delmem = malloc(sizeof(DeleteMemento));
    delmem->args = malloc(sizeof(char *) * (argc + 1));

    for(int i = 0; args[i + 1] != NULL; i++){
        delmem->args[i] = strdup(args[i + 1]);
    }

    delmem->args[argc] = NULL;

    HollowMemento holmem;
    holmem.memtype = MEM_DELETE;
    holmem.mem.delmem = delmem;

    upush(holmem);
}

void create_intmem(char **args, int arg_idx, int argc){
    InternalMemento intmem;

    char buf[PATH_MAX];
    if(getcwd(buf, sizeof(buf)) != NULL){
        intmem.cwd = strdup(buf);
    }

    HollowMemento holmem;
    holmem.type = CMD_INTERNAL;
    holmem.mem.intmem = intmem;

    upush(holmem);
}

//with this function the memento is created and is pushed onto the stack
void create_holmem(char **args){
    if(args == NULL || args[0] == NULL) return;

    int arg_idx = 0, argc = 0;
    for(int i = 0; args[i] != NULL; i++){
        if(arg_idx == 0 && args[i][0] != '-'){
            arg_idx = i;
        }
        argc++;
    }

    Cmdtype type = get_cmdtype(args[0]);

    switch(type){
        case CMD_CREATE_FILE:
            create_creatmem(args, arg_idx, argc, CMD_CREATE_FILE);

        case CMD_CREATE_DIR:
            create_creatmem(args, arg_idx, argc, CMD_CREATE_DIR);

        case CMD_MOVE:
            create_movmem(args, arg_idx, argc);
 
        case CMD_DELETE:
            create_delmem(args, arg_idx, argc);

        case CMD_INTERNAL:
            create_intmem(args, arg_idx, argc);

        case CMD_NOUNDO:
            return;
    }
}

void free_creatmem(CreateMemento *creatmem){

    for(int i = 0; creatmem->args[i] != NULL; i++){
        free(creatmem->args[i]);
    }

    free(creatmem->args);
}

void free_movmem(MoveMemento *movmem){

    free(movmem->dest);
    free(movmem->source);
}

void free_delmem(DeleteMemento *delmem){

    for(int i = 0; delmem->args[i] != NULL; i++){
        free(delmem->args[i]);
    }

    free(delmem->args);
}

void free_intmem(InternalMemento *intmem){

    free(intmem->cwd);
}

void free_holmem(HollowMemento holmem){
    switch(holmem.type){
        case CMD_CREATE:
            free_creatmem(&holmem.mem.creatmem);

        case CMD_MOVE:
            free_movmem(&holmem.mem.movmem);

        case CMD_DELETE:
            free_delmem(&holmem.mem.delmem);

        case CMD_INTERNAL:
            free_intmem(&holmem.mem.intmem);

        case CMD_NOUNDO:
            return;
    }
}

void upush(HollowMemento holmem){
    if(utop >= MAX_USTACK - 1){

        free_holmem(ustack[0]);

        for(int i = 0; i < MAX_USTACK - 1; i++){
            ustack[i] = ustack[i + 1];
        }

        utop = MAX_USTACK - 1;

        ustack[utop] = holmem;
    }
    else{
        utop++;
        ustack[utop] = holmem;
    }
}

void get_timestamps(char *name, struct timeval *time){
    struct stat st;
    stat(name, &st);

    time[0].tv_sec = st.st_atime;
    time[0].tv_usec = 0;

    time[1].tv_sec = st.st_mtime;
    time[1].tv_usec = 0;
}

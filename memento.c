#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libgen.h>
#include <time.h>

#include "memento.h"

#define MAX_USTACK 40

typedef enum Cmdtype{
    CMD_MOVE,
    CMD_DELETE,
    CMD_INTERNAL,
    CMD_CREATE_FILE,
    CMD_CREATE_DIR,
    CMD_NOUNDO
}Cmdtype;

typedef struct CmdRule{
    char *name;
    Cmdtype type;
}CmdRule;

typedef struct CreateMemento{
    char **paths;
    char **parent_dirs;
    int num_paths;

    bool is_dir;

    struct timeval (*dir_times)[2];
}CreateMemento;

typedef struct MoveMemento{
    char *source;
    char *dest;

    char *source_pdir;
    char *dest_pdir;

    struct timeval spdir_time[2];
    struct timeval dpdir_time[2];
}MoveMemento;

typedef struct DeleteMemento{
    char* (*paths)[2];
    char **parent_dirs;
    int num_paths;

    struct timeval (*parent_times)[2];
}DeleteMemento;

typedef struct InternalMemento{
    char *cwd;
}InternalMemento;

void push(HollowMemento holmem);
void get_timestamps(char *name, struct timeval *time);
char *trash(char *file);

static HollowMemento ustack[MAX_USTACK];
static int utop = -1;

static char trash_path[PATH_MAX];

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

void create_creatmem(char **args, int arg_idx, int argc, HollowMemento *holmem, Cmdtype type){
    int num_paths = argc - arg_idx;

    CreateMemento *creatmem = malloc(sizeof(CreateMemento));

    creatmem->paths = malloc(num_paths * sizeof(char *));
    creatmem->parent_dirs = malloc(num_paths * sizeof(char *));
    creatmem->dir_times = malloc(sizeof(struct timeval) * num_paths * 2);

    creatmem->is_dir = (type == CMD_CREATE_DIR);
    creatmem->num_paths = num_paths;

    for(int i = 0; args[i + arg_idx] != NULL; i++){
        creatmem->paths[i] = strdup(args[i + arg_idx]);

        char tmp_path[PATH_MAX];
        snprintf(tmp_path, sizeof(tmp_path), "%s", args[i + arg_idx]);

        char *dir = dirname(tmp_path);
        creatmem->parent_dirs[i] = strdup(dir);

        get_timestamps(dir, creatmem->dir_times[i]);
    }

    holmem->memtype = MEM_CREATE;
    holmem->mem.creatmem = creatmem;
}

void create_movmem(char **args, int arg_idx, int argc, HollowMemento *holmem){
    (void)argc;

    MoveMemento *movmem = malloc(sizeof(MoveMemento));
    movmem->source = strdup(args[arg_idx]);
    movmem->dest = strdup(args[arg_idx + 1]);

    char tmp_path[PATH_MAX];

    snprintf(tmp_path, sizeof(tmp_path), "%s", args[arg_idx]);
    char *dir = dirname(tmp_path);
    movmem->source_pdir = strdup(dir);

    get_timestamps(dir, movmem->spdir_time);

    snprintf(tmp_path, sizeof(tmp_path), "%s", args[arg_idx + 1]);
    dir = dirname(tmp_path);
    movmem->dest_pdir = strdup(dir);

    get_timestamps(dir, movmem->dpdir_time);

    holmem->memtype = MEM_MOVE;
    holmem->mem.movmem = movmem;
}

void create_delmem(char **args, int arg_idx, int argc, HollowMemento *holmem){
    int num_paths = argc - arg_idx;

    DeleteMemento *delmem = malloc(sizeof(DeleteMemento));

    delmem->paths = malloc(num_paths * sizeof(*delmem->paths));
    delmem->parent_dirs = malloc(num_paths * sizeof(char *));
    delmem->parent_times = malloc(sizeof(*delmem->parent_times) * num_paths);

    delmem->num_paths = num_paths;

    for(int i = 0; args[i + arg_idx] != NULL; i++){
        delmem->paths[i][0] = strdup(args[i + arg_idx]);
        delmem->paths[i][1] = trash(args[i + arg_idx]);

        char tmp_path[PATH_MAX];
        snprintf(tmp_path, sizeof(tmp_path), "%s", args[i + arg_idx]);

        char *dir = dirname(tmp_path);
        delmem->parent_dirs[i] = strdup(dir);

        get_timestamps(dir, delmem->parent_times[i]);
    }

    holmem->memtype = MEM_DELETE;
    holmem->mem.delmem = delmem;
}

void create_intmem(char **args, int arg_idx, int argc, HollowMemento *holmem){
    (void)args;
    (void)arg_idx;
    (void)argc;

    InternalMemento *intmem = malloc(sizeof(InternalMemento));

    char buf[PATH_MAX];
    if(getcwd(buf, sizeof(buf)) != NULL){
        intmem->cwd = strdup(buf);
    }

    holmem->memtype = MEM_INTERNAL;
    holmem->mem.intmem = intmem;
}

void create_holmem(char **args, HollowMemento *holmem){
    if(args == NULL || args[0] == NULL){
        return;
    }

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
            create_creatmem(args, arg_idx, argc, holmem, CMD_CREATE_FILE);
            break;

        case CMD_CREATE_DIR:
            create_creatmem(args, arg_idx, argc, holmem, CMD_CREATE_DIR);
            break;

        case CMD_MOVE:
            create_movmem(args, arg_idx, argc, holmem);
            break;

        case CMD_DELETE:
            create_delmem(args, arg_idx, argc, holmem);
            break;

        case CMD_INTERNAL:
            create_intmem(args, arg_idx, argc, holmem);
            break;

        case CMD_NOUNDO:
            holmem->memtype = MEM_INV;
            return;
    }
}

void free_creatmem(CreateMemento *creatmem){
    for(int i = 0; i < creatmem->num_paths; i++){
        free(creatmem->paths[i]);
        free(creatmem->parent_dirs[i]);
    }

    free(creatmem->paths);
    free(creatmem->parent_dirs);
    free(creatmem->dir_times);

    free(creatmem);
}

void free_movmem(MoveMemento *movmem){
    free(movmem->dest);
    free(movmem->source);

    free(movmem->source_pdir);
    free(movmem->dest_pdir);

    free(movmem);
}

void free_delmem(DeleteMemento *delmem){
    for(int i = 0; i < delmem->num_paths; i++){
        free(delmem->paths[i][0]);
        free(delmem->paths[i][1]);
        free(delmem->parent_dirs[i]);
    }

    free(delmem->paths);
    free(delmem->parent_dirs);
    free(delmem->parent_times);

    free(delmem);
}

void free_intmem(InternalMemento *intmem){
    free(intmem->cwd);

    free(intmem);
}

void free_holmem(HollowMemento holmem){
    switch(holmem.memtype){
        case MEM_CREATE:
            free_creatmem(holmem.mem.creatmem);
            break;

        case MEM_MOVE:
            free_movmem(holmem.mem.movmem);
            break;

        case MEM_DELETE:
            free_delmem(holmem.mem.delmem);
            break;

        case MEM_INTERNAL:
            free_intmem(holmem.mem.intmem);
            break;

        case MEM_INV:
            return;
    }
}

void push(HollowMemento holmem){
    if(holmem.memtype == MEM_INV) return;

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

void pop(){
    if(utop < 0) return;

    free_holmem(ustack[utop]);
    utop--;
}

void get_timestamps(char *name, struct timeval *time){
    struct stat st;
    stat(name, &st);

    time[0].tv_sec = st.st_atime;
    time[0].tv_usec = 0;

    time[1].tv_sec = st.st_mtime;
    time[1].tv_usec = 0;
}

//WHat if a user uses a relative path, changes dir and tries to undo? Expand paths to abs paths when creating the memento
void undo_creatmem(CreateMemento *creatmem, bool is_dir){
    if(is_dir){
        for(int i = 0; i < creatmem->num_paths; i++){
            rmdir(creatmem->paths[i]);

            utimes(creatmem->parent_dirs[i], creatmem->dir_times[i]);
        }
    }
    else{
        for(int i = 0; i < creatmem->num_paths; i++){
            unlink(creatmem->paths[i]);

            utimes(creatmem->parent_dirs[i], creatmem->dir_times[i]);
        }
    }
}

void undo_movmem(MoveMemento *movmem){
    rename(movmem->dest, movmem->source);

    utimes(movmem->source_pdir, movmem->spdir_time);
    utimes(movmem->dest_pdir, movmem->dpdir_time);
}

void undo_delmem(DeleteMemento *delmem){
    for(int i = 0; i < delmem->num_paths; i++){
        rename(delmem->paths[i][1], delmem->paths[i][0]);

        utimes(delmem->parent_dirs[i], delmem->parent_times[i]);
    }
}

void undo_intmem(InternalMemento *intmem){
    chdir(intmem->cwd);
}

int undo(char **args){
    (void)args;

    if(utop < 0) return 1;
    HollowMemento holmem = ustack[utop];

    switch(holmem.memtype){
        case MEM_CREATE:
            undo_creatmem(holmem.mem.creatmem, holmem.mem.creatmem->is_dir);
            break;

        case MEM_MOVE:
            undo_movmem(holmem.mem.movmem);
            break;

        case MEM_DELETE:
            undo_delmem(holmem.mem.delmem);
            break;

        case MEM_INTERNAL:
            fprintf(stderr, "undoing intmem\n");
            undo_intmem(holmem.mem.intmem);
            break;

        default:
            fprintf(stderr, "memtype not found\n");
    }

    pop();

    return 0;
}

int init_memento(){
    srand(time(NULL));

    char *home = getenv("HOME");
    if(home != NULL){
        char shell_dir[PATH_MAX];
        snprintf(shell_dir, sizeof(shell_dir), "%s/.local/share/hsh", home);

        struct stat st;
        if(stat(shell_dir, &st) != 0){
            mkdir(shell_dir, 0700);
        }
        else if(S_ISDIR(st.st_mode) == 0){
            fprintf(stderr, "hsh: Failed to create shell directory. A file with the same name exists\n");
            return 1;
        }

        snprintf(trash_path, sizeof(trash_path), "%s/.local/share/hsh/trash", home);

        if(stat(trash_path, &st) != 0){
            mkdir(trash_path, 0700);
        }
        else if(S_ISDIR(st.st_mode) == 0){
            fprintf(stderr, "hsh: Failed to create shell trash directory. A file with the same name exists\n");
            return 1;
        }
    }

    return 0;
}

// TODO COPY THE FILE DONT MOVE IT
// Also add commands for emtpying the trash dir and printing it's contents
char *trash(char *file){
    char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char rname[9];

    for(int i = 0; i < 8; i++){
        int rindex = rand() % strlen(chars);
        rname[i] = chars[rindex];
    }

    rname[8] = '\0';

    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/%s", trash_path, rname);

    rename(file, buf);

    return strdup(buf);
}

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/sysmacros.h>

#define MAX_FNAME 256

typedef enum Action{
    F_CREATE,
    F_DELETE,
    F_WRITE,
    DIR_CREATE,
    DIR_DELETE,
    DIR_MOD,
    ACT_INV
}Action;

typedef struct Memento{
    char *path;
    char *tpath;
    mode_t mode;
    uid_t uid;
    gid_t gid;

    Action action;
}Memento;

typedef struct HollowMemento{
    Memento **mementos;
}HollowMemento;

char *hshdir = "/var/lib/hsh/";

char *workdir = "/var/lib/hsh/work/";
char *upperdir = "/var/lib/hsh/upper/";
char *mergeddir = "/var/lib/hsh/merged/";
char *lowerdir = "/";

void reap_dir(const char *dir_path, HollowMemento *holmem);
Action find_action(const char *upper_path);

//TODO maybe leaving the timestamps untouched is better?

int init_ovl_dirs(){
    struct stat st;
    if(stat(hshdir, &st) != 0){
        mkdir(hshdir, 0700);
    }
    else if(S_ISDIR(st.st_mode) == 0){
        fprintf(stderr, "hsh: Failed to create shell directory. A file with the same name exists\n");
        return 1;
    }

    mkdir(workdir, 0700);
    mkdir(upperdir, 0700);
    mkdir(mergeddir, 0700);

    return 0;
}

//iterate through upperdir
//on every file/dir run a helper which will return the Action type
//create the Memento, store the path and the action type
//wrap the holmem around the memento(s)
//push on the stack

void reap_overlay(){
    HollowMemento *holmem = malloc(sizeof(HollowMemento));

    reap_dir(upperdir, holmem);
}

void reap_dir(const char *dir_path, HollowMemento *holmem){
    DIR *dir = opendir(dir_path);

    struct dirent *entry = NULL;
    while((entry = readdir(dir)) != NULL){
        char abs_path[MAX_FNAME];
        snprintf(abs_path, MAX_FNAME, "%s/%s", dir_path, entry->d_name);

        if(entry->d_type == DT_DIR){
            if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
                reap_dir(abs_path, holmem);
                //reap the directory itself here and commit after recursion ends
            }
        }
        else{
            Action action = find_action(abs_path);
            
        }
    }
}

//upper_path is for stat-ing files in upperdir, lower_path is for stat-ing files in lowerdir 
Action find_action(const char *upper_path){
    size_t lower_sz = strlen(upper_path); //this will leave some extra space
    char lower_path[lower_sz];

    snprintf(lower_path, lower_sz, "%s", upper_path + strlen(upperdir) - 1);

    struct stat st_upper, st_lower;
    if(lstat(upper_path, &st_upper) != 0){
        return ACT_INV;
    }

    if(lstat(lower_path, &st_lower) != 0){
        if(S_ISDIR(st_upper.st_mode)){
            return DIR_CREATE;
        }
        else{
            return F_CREATE;
        }
    }
    else if(S_ISCHR(st_upper.st_mode) && (major(st_upper.st_rdev) == 0 && minor(st_upper.st_rdev) == 0)){
        if(S_ISDIR(st_lower.st_mode)){
            return DIR_DELETE;
        }
        else{
            return F_DELETE;
        }
    }
    else{
        if(S_ISDIR(st_upper.st_mode)){
            return DIR_MOD;
        }
        else{
            return F_WRITE;
        }
    }
}

Memento *create_mem(Action action, const char *upper_path){
    Memento *mem = malloc(sizeof(Memento));

    size_t lower_sz = strlen(upper_path);
    char lower_path[lower_sz];

    snprintf(lower_path, lower_sz, "%s", upper_path + strlen(upperdir) - 1);

    mem->path = strdup(lower_path);
    mem->action = action;
    switch(action){
        case F_CREATE:
            break;

        case DIR_CREATE:
            break;

        case F_WRITE:
        case F_DELETE:{
            //mem->tpath = trash(lower_path);

            struct stat st;
            if(lstat(lower_path, &st) != 0){
                return NULL;
            }

            mem->mode = st.st_mode & 07777;
            mem->uid = st.st_uid;
            mem->gid = st.st_gid;

            break;
        }

        case DIR_DELETE:
        case DIR_MOD:{
            struct stat st;
            if(lstat(lower_path, &st) != 0){
                free(mem);
                return NULL;
            }

            mem->mode = st.st_mode & 07777;
            mem->uid = st.st_uid;
            mem->gid = st.st_gid;

            break;
        }

        case ACT_INV:
            return NULL;
    }

    return NULL;
}

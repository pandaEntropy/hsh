#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

#define MAX_FNAME 256

typedef enum Action{
    F_CREATE,
    F_DELETE,
    F_WRITE,
    DIR_CREATE,
    DIR_DELETE
}Action;

typedef struct Memento{
    char *path;
    Action action;
    //maybe you could have the trash file path here as well? It would be NULL if not needed
}Memento;

typedef struct HollowMemento{
    Memento mementos[];
}HollowMemento;

char *hshdir = "/var/lib/hsh/";

char *workdir = "/var/lib/hsh/work/";
char *upperdir = "/var/lib/hsh/upper/";
char *mergeddir = "/var/lib/hsh/merged/";
char *lowerdir = "/";

void reap_dir(const char *dir_path, HollowMemento *holmem);

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
            //reap the file and also commit it here
        }
    }
}

//path should be the path relative to upperdir.
//If it was abs path, it would give the file in upperdir when I stat it. But I need the actual one in lowerdir.
//TODO you might need to prepend a "/" to path
Action find_action(const char *path, struct dirent *entry){
    struct stat st;
    if(stat(path, &st) != 0){ //Not in lowerdir
        if(entry->d_type == DT_DIR){
            return DIR_CREATE;
        }
        else{
            return F_CREATE;
        }
    }

    return F_WRITE;
}
//TODO pick up from character devices and whiteouts

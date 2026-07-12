#include <sys/stat.h>
#include <stdio.h>

typedef enum Action{
    CREATE_F,
    DELETE_F,
    CREATE_DIR,
    DELETE_DIR
}Action;

typedef struct Memento{
    Action act;
    char *path;
}Memento;

typedef struct HollowMemento{
    Memento mementos[];
}HollowMemento;

char *hshdir = "/var/lib/hsh/";

char *workdir = "/var/lib/hsh/work/";
char *upperdir = "/var/lib/hsh/upper/";
char *mergeddir = "/var/lib/hsh/merged/";
char *lowerdir = "/";

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
void create_holmem(){
    
}

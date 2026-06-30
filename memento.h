#ifndef MEMENTO_H
#define MEMENTO_H

typedef struct CreateMemento CreateMemento;
typedef struct MoveMemento MoveMemento;
typedef struct DeleteMemento DeleteMemento;
typedef struct InternalMemento InternalMemento;

typedef enum MemType{
    MEM_CREATE,
    MEM_MOVE,
    MEM_DELETE,
    MEM_INTERNAL,
    MEM_INV
}MemType;

typedef struct HollowMemento{
    MemType memtype;
    union{
        CreateMemento *creatmem;
        MoveMemento *movmem;
        DeleteMemento *delmem;
        InternalMemento *intmem;
    }mem;
}HollowMemento;

int init_memento();
void create_holmem(char **args, HollowMemento *holmem);
void push(HollowMemento holmem);
int undo(char **args);
void free_holmem(HollowMemento holmem);
void mem_cleanup();

#endif

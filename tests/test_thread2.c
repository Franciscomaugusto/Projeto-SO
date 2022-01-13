#include "../fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define SIZE  250
#define REPS 100

struct params{
    int fd;
    char* str;
    size_t size;
};

void * tfs_thread(void* param){
    struct params* params1 = (struct params*) param;
    tfs_write(params1->fd, params1->str, params1->size);
    return NULL;
}

int main() {

    char *path = "/f1";
    char *str = "AAA!!!!A!!!?BBBBBBBB123455678910cccccCCCCC123456778910CC!!!!!!!!!!!?!~AAA!!!!A!!!?BBBBBBBB123455678910cccccCCCCC123456778910CC!!!!!!!!!!!?!~AAA!!!!A!!!?BBBBBBBB123455678910cccccCCCCC123456778910CC!!!!!!!!!!!?!~AAA!!!!A!!!Estou aqui para marcar o fim!";

    char input[SIZE] ="AAA!!!!A!!!?BBBBBBBB123455678910cccccCCCCC123456778910CC!!!!!!!!!!!?!~AAA!!!!A!!!?BBBBBBBB123455678910cccccCCCCC123456778910CC!!!!!!!!!!!?!~AAA!!!!A!!!?BBBBBBBB123455678910cccccCCCCC123456778910CC!!!!!!!!!!!?!~AAA!!!!A!!!Estou aqui para marcar o fim!";
    char output [SIZE];
    pthread_t tid[REPS];
    struct params estrutura;

    assert(tfs_init() != -1);

    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    estrutura.fd = fd;
    estrutura.str = str;
    estrutura.size = 250;
    for (int i = 0; i < REPS; i++) {
        pthread_create(&tid[i], NULL,tfs_thread, &estrutura);
    }
    for (int i = 0; i < REPS; i++) {
        pthread_join(tid[i], NULL);
    }
    assert(tfs_close(fd)!=-1);
    assert(tfs_open(path, 0)!=-1);
    
    for(int i = 0; i < REPS; i++){
        assert(tfs_read(fd, output, SIZE) == SIZE);
        assert(memcmp(input, output, SIZE) == 0);
    }

    assert(tfs_close(fd) != -1);

    printf("Successful test.\n");
    return 0;
}
#include "operations.h"
#include <string.h>

#define CLIENT_LIMIT 10
#define NAME_LIMIT 40
 
struct message{
    int message;
    char opcode;
    char* name;
    int flags;
    int fhandle;
    size_t len;
    char* buffer;
};

struct message messenger[CLIENT_LIMIT];
static pthread_mutex_t locks[CLIENT_LIMIT];
static pthread_cond_t signals[CLIENT_LIMIT];


int client_list[CLIENT_LIMIT];
int server_shutdown = 0;
int client_num = 0;
pthread_t workers[CLIENT_LIMIT];


void client_error(int id){
    int cliente = client_list[id];
    client_list[id] = -1;
    close(cliente);
}


void * worker_thread(void* id){
    int identificador = *((int*) id);
    int thread_shutdown = 0;
    while(thread_shutdown == 0){
        if(pthread_mutex_lock(&locks[identificador]) != 0){
            
        }
        while(messenger[identificador].message  == 0){
            pthread_cond_wait(&signals[identificador], &locks[identificador]);
        }
        if(messenger[identificador].opcode == '2'){
            int unmounting = client_list[identificador];
            client_list[identificador] = -1;
            client_num--;
            close(unmounting);
            messenger[identificador].message = 0;
        }

        if(messenger[identificador].opcode == '3'){
            int opening = client_list[identificador];
            int open = tfs_open(messenger[identificador].name, messenger[identificador].flags);
            if(write(opening, &open, sizeof(int)) < sizeof(int)){
                client_error(opening);
            }
            free(messenger[identificador].name);
            messenger[identificador].message = 0;
        }

       if(messenger[identificador].opcode == '4'){
            int closing = client_list[identificador];
            int fhandle = messenger[identificador].fhandle;
            int close = tfs_close(fhandle);
            if(write(closing, &close, sizeof(int)) == -1){
                client_error(closing);
            }
            messenger[identificador].message = 0;
        }

       if(messenger[identificador].opcode == '5'){
            int writing = client_list[identificador];
            int file = messenger[identificador].fhandle;
            size_t len = messenger[identificador].len;
            char* buffer = messenger[identificador].buffer;
            ssize_t written =tfs_write(file, (void*) buffer, len);
            if(write(writing, &written, sizeof(ssize_t)) == -1){
                client_error(writing);
            }
            free(messenger[identificador].buffer);
            messenger[identificador].message = 0;
        }

        if(messenger[identificador].opcode == '6'){
            int reading = client_list[identificador];
            int ficheiro = messenger[identificador].fhandle;
            size_t len = messenger[identificador].len;
            char *reader = (char*) malloc((len) * sizeof(char));
            ssize_t I_read = tfs_read(ficheiro, reader, len);
            if(write(reading, &I_read, sizeof(ssize_t)) == -1){
                client_error(reading);
            }
            for(long unsigned int i=0; i< I_read; i++){
                if(write(reading, &reader[i], sizeof(char)) == -1){
                    client_error(reading);
                }
            }
            free(reader);
            messenger[identificador].message = 0;
        }

        if(messenger[identificador].opcode == '7'){
            int shutdown = client_list[identificador];
            int result = tfs_destroy_after_all_closed();
            if(write(shutdown, &result, sizeof(int)) == -1){
                client_error(shutdown);
            }
            server_shutdown = 1;
            messenger[identificador].message = 0;
        }
        if(pthread_mutex_unlock(&locks[identificador]) != 0){
            
        }
    }
    return NULL;
}


int main(int argc, char **argv) {
    int session_id;
    char opcode;
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }
    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);
    if (unlink(pipename) != 0 && errno != ENOENT) {
        return -1;
    }
    if (mkfifo(pipename, 0640) != 0) {
        return -1;
    }
    int receiver = open(pipename, O_RDONLY);
    if (receiver == -1) {
        return -1;
    }
    int *array = (int*) malloc(CLIENT_LIMIT * sizeof(int));
    for(int i = 0; i<CLIENT_LIMIT; i++){
        messenger[i].message = 0;
        client_list[i] = -1;
        array[i] = i;
        pthread_create(&workers[i], NULL,worker_thread,&array[i]);
    }
    free(array);
    tfs_init();
    while (server_shutdown == 0){
        if(read(receiver,&opcode, sizeof(char)) == -1){
            server_shutdown = 1;
        }
        if(opcode == '1'){
            int client;
            int livre;
            char buffer[NAME_LIMIT +1];
            if(read(receiver, &buffer, NAME_LIMIT) == -1){
                server_shutdown = 1;
            }
            buffer[NAME_LIMIT] = '\0';
            client = open(buffer, O_WRONLY);
            livre = -1;
            if(client_num < CLIENT_LIMIT){
                for(int i= 0; i<CLIENT_LIMIT; i++){
                    if(client_list[i] == -1){
                        livre = i;
                        break;
                    }
                }
                client_list[livre] = client;
                client_num++;
                if(write(client, &livre, sizeof(int)) == -1){
                    client_error(client);
                }
                opcode = '\0';
            }else{
                if(write(client, &livre, sizeof(int)) == -1){
                    client_error(client);
                }
            }
        }

        if(opcode == '2'){
            int unmount;
            if(read(receiver,&unmount,sizeof(int)) == -1){
                server_shutdown = 1;
            }
            messenger[session_id].opcode = '2';
            messenger[session_id].message = 1;
            pthread_cond_signal(&signals[session_id]);
            opcode = '\0';
        }

        if(opcode == '3'){
            int flags;
            char name[NAME_LIMIT +1];
            ssize_t lidos;
            if(read(receiver,&session_id,sizeof(int)) <sizeof(int)){
                server_shutdown = 1;
            }
            messenger[session_id].opcode = '3';
            lidos = read(receiver, name, NAME_LIMIT);
            if(lidos == -1){
                server_shutdown = 1;
            }
            name[lidos] = '\0';
            char* nome = (char*) malloc((long unsigned int)lidos * sizeof(char));
            memcpy(nome, name, (long unsigned int)lidos);
            messenger[session_id].name = nome;
            if(read(receiver, &flags, sizeof(int)) < sizeof(int)){
               server_shutdown = 1;
            }
            messenger[session_id].flags = flags;
            messenger[session_id].message = 1;
            pthread_cond_signal(&signals[session_id]);
            opcode='\0';
        }

       if(opcode == '4'){
           if(read(receiver,&session_id,sizeof(int))== -1){
               server_shutdown = 1;
            }
            messenger[session_id].opcode = '4';
            int fhandle;
            if(read(receiver,&fhandle, sizeof(int)) == -1){
                server_shutdown = 1;
            }
            messenger[session_id].fhandle = fhandle;
            messenger[session_id].message = 1;
            pthread_cond_signal(&signals[session_id]);
            opcode='\0';
        }

       if(opcode == '5'){
            if(read(receiver,&session_id,sizeof(int)) == -1){
                server_shutdown = 1;
            }
            messenger[session_id].opcode = '5';
            int file;
            size_t len;
            if(read(receiver, &file, sizeof(int)) == -1){
                server_shutdown = 1;
            }
            messenger[session_id].fhandle = file;
            if(read(receiver, &len, sizeof(size_t)) == -1){
               server_shutdown = 1;
            }
            messenger[session_id].len= len;
            char buffer[len];
            char *vector = (char*) malloc(len * sizeof(char));
            if(read(receiver, buffer, len) == -1){
                client_error(session_id);
            }
            memcpy(vector,buffer , len);
            messenger[session_id].buffer = vector;
            messenger[session_id].message = 1;
            pthread_cond_signal(&signals[session_id]);
            opcode = '\0';
        }

        if(opcode == '6'){
            if(read(receiver,&session_id,sizeof(int)) == -1){
                server_shutdown = 1;
            }
            messenger[session_id].opcode = '6';
            int ficheiro;
            size_t len;
            if(read(receiver, &ficheiro, sizeof(int)) == -1){
                server_shutdown = 1;
            }
            messenger[session_id].fhandle = ficheiro;
            if(read(receiver, &len, sizeof(size_t)) == -1){
                server_shutdown = 1;
            }
            messenger[session_id].len = len;
            messenger[session_id].message = 1;
            pthread_cond_signal(&signals[session_id]);
            opcode = '\0';
        }

        if(opcode == '7'){
            if(read(receiver,&session_id,sizeof(int)) == -1){
                server_shutdown = 1;
            }
            messenger[session_id].opcode = '7';
            messenger[session_id].message = 1;
            pthread_cond_signal(&signals[session_id]);
        }
    }
    close(receiver);
    if (unlink(pipename) != 0 && errno != ENOENT) {
        return -1;
    }
    return 0;
}

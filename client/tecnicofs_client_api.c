#include "tecnicofs_client_api.h"

int session_id, receiver, sender;
char const *pipe_path;


int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    char buffer[41];
    void* pointer;
    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }
    pipe_path = client_pipe_path;
    if (mkfifo(client_pipe_path, 0640) != 0) {
        return -1;
    }
    sender = open(server_pipe_path, O_WRONLY);
    if(sender == -1){
        return -1;
    }
    buffer[0] = '1';
    pointer = &buffer[1];
    memcpy( pointer ,client_pipe_path, strlen(client_pipe_path));
    if(strlen(client_pipe_path) < 40){
        long unsigned int a_adicionar = 40 -strlen(client_pipe_path);
        long unsigned int inicio = strlen(client_pipe_path);
        while(a_adicionar >0){
            buffer[inicio] += '\0';
            a_adicionar--;
            inicio++;
        }
    }
    ssize_t resp = write(sender,buffer, 41);
    if(resp < 0){
        return -1;
    }
    receiver = open(client_pipe_path, O_RDONLY);
    if(receiver == -1){
        return -1;
    }
    if(read(receiver, &session_id, sizeof(int)) == -1){
        return -1;
    }
    if(session_id == -1){
        return -1;
    }
    return 0;
}


int tfs_unmount(){
    char message[5];
    message[0] = '2';
    memcpy(message +1, &session_id, sizeof(int));
    if(write(sender, message, 5) == -1){
        return -1;
    }
    close(sender);
    if (unlink(pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }
    return 0;
}


int tfs_open(char const *name, int flags) {
    if(strlen(name) < 40){
        long unsigned int a_adicionar = 40 -strlen(name);
        while(a_adicionar >0){
            name += '\0';
            a_adicionar--;
        }
    }
    char message[49];
    message[0] = '3';
    int fhandle;
    memcpy(message +1, &session_id, sizeof(int));
    memcpy(message+5, name, 40);
    memcpy(message+45, &flags, sizeof(int));
    if(write(sender, message, 49) == -1){
        return -1;
    }
    if(read(receiver, &fhandle, sizeof(int)) == -1){
        return -1;
    }
    return fhandle;
}


int tfs_close(int fhandle) {
    int closed = -1;
    char message[9];
    message[0] = '4';
    memcpy(message +1, &session_id, sizeof(int));
    memcpy(message+5, &fhandle, sizeof(int));
    if(write(sender, message, 9) == -1){
        return -1;
    }if(read(receiver, &closed, sizeof(int)) == -1){
        return -1;
    }
    return closed;
}


ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    ssize_t written;
    char message[17+ len];
    message[0] = '5';
    memcpy(message +1, &session_id, sizeof(int));
    memcpy(message +5, &fhandle, sizeof(int));
    memcpy(message +9, &len, sizeof(size_t));
    memcpy(message +17, buffer, len);
    if(write(sender,message, 17 + len) == -1){
        return -1;
    }
    if(read(receiver, &written, sizeof(ssize_t)) == -1){
        return -1;
    }
    return written;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    size_t I_read;
    char message[17];
    message[0] = '6';
    memcpy(&message[1], &session_id, sizeof(int));
    memcpy(&message[5], &fhandle, sizeof(int));
    memcpy(&message[9], &len, sizeof(size_t));
    if(write(sender, message, 17) == -1){
        return -1;
    }
    if(read(receiver, &I_read, sizeof(ssize_t)) == -1){
        return -1;
    }
    char lido[I_read];
    if(read(receiver, lido, I_read) == -1){
            return -1;
    }
    memcpy(buffer, lido, I_read);
    return (ssize_t) I_read;
}


int tfs_shutdown_after_all_closed() {
    char message[5];
    message[0] = '7';
    memcpy(&message[1], &session_id, sizeof(int));
    int result;
    if(write(sender, message, 5) == -1){
        return -1;
    }
    if(read(receiver, &result, sizeof(int)) == -1){
        return -1;
    }
    return result;
}
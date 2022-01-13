#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}


int tfs_destroy() {
    state_destroy();
    return 0;
}


static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}


int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}


int tfs_open(char const *name, int flags) {
    int inum, i;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        pthread_rwlock_wrlock( &inode->lock);
        if (inode == NULL) {
            return -1;
        }

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                for(i = 0; i <10; i++){
                    if (data_block_free(inode->i_data_block[i]) == -1) {
                        pthread_rwlock_unlock( &inode->lock);
                        return -1;
                    }
                }
            if(inode->i_data_block[10] !=-1){
                if (free_reference_block(inode->i_data_block[10]) == -1) {
                    pthread_rwlock_unlock( &inode->lock);
                    return -1;
                }
                inode->i_data_block[10] = -1;
            }
                inode->i_size = 0;
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }


size_t calculate_more_block(inode_t* inode, size_t to_write){
    size_t blocks_to_write=0, blocks_to_alloc=0;
    size_t incremento = (to_write)/BLOCK_SIZE;
    blocks_to_alloc = incremento;
    for(int i =0; i< 10; i++){
        if(inode-> i_data_block[i] == -1 && (incremento != 0)){
            inode-> i_data_block[i] = data_block_alloc();
            blocks_to_write++;
            blocks_to_alloc--;
            incremento--;
        }
    }
    if((BLOCK_SIZE * blocks_to_alloc) < to_write){
        size_t disponivel = BLOCK_SIZE - ((inode-> i_size)%BLOCK_SIZE);
        if(disponivel != 0){
            blocks_to_write++;
        }
        if(disponivel < (to_write - (BLOCK_SIZE * blocks_to_alloc)) || (inode->i_size) == 0){
            blocks_to_alloc++;
            bool adicionado =false;
            for(int i = 0; i< 10; i++){
                if(inode->i_data_block[i] == -1 && !adicionado){
                    inode->i_data_block[i] = data_block_alloc();
                    adicionado = true;
                    blocks_to_write++;
                    blocks_to_alloc--;
                }
            }
        }
    }
    if( blocks_to_alloc > 0){
        if(inode ->i_data_block[10] == -1){
            inode ->i_data_block[10] = data_block_alloc();
            int* pointer = (int*) data_block_get(inode -> i_data_block[10]);
            for(int i =0; i<(BLOCK_SIZE/sizeof(int)); i++){
                pointer[i] = -1;
            }
        } 
        int* pointer = (int*) data_block_get(inode -> i_data_block[10]);
        int index = 0;
        while(blocks_to_alloc != 0){
            if(pointer[index] != -1){
                index++;
            }
            else{
                pointer[index] = data_block_alloc();
                blocks_to_write++;
                blocks_to_alloc--;
            }
        }
    }
    return blocks_to_write;
}


size_t calculate_less_block(inode_t* inode, size_t to_write){
    size_t blocks_to_write=0;
    bool adicionado = false;
    if( BLOCK_SIZE - (inode->i_size %BLOCK_SIZE) < to_write){
        for(int i = 0; i< 10; i++){
            if(inode->i_data_block[i] == -1 && !adicionado){
                inode->i_data_block[i] = data_block_alloc();
                blocks_to_write++;
                adicionado = true;
            }
        }
        blocks_to_write++;
    }
    if(inode->i_size % BLOCK_SIZE == 0){
        for(int i = 0; i< 10; i++){
            if(inode->i_data_block[i] == -1 && !adicionado){
                inode->i_data_block[i] = data_block_alloc();
                blocks_to_write++;
                adicionado = true;
            }
        }
    }
    if(BLOCK_SIZE - (inode->i_size % BLOCK_SIZE) >= to_write && (inode->i_size % BLOCK_SIZE) != 0){
        blocks_to_write++;
        adicionado = true;
    }
    if(!adicionado){
        if(inode ->i_data_block[10] == -1){
            inode ->i_data_block[10] = data_block_alloc();
            int* pointer = (int*) data_block_get(inode -> i_data_block[10]);
            for(int i =0; i<(BLOCK_SIZE/sizeof(int)); i++){
                pointer[i] = -1;
            }
        } 
        int* pointer = (int*) data_block_get(inode -> i_data_block[10]);
        int index = 0;
        while(!adicionado){
            if(pointer[index] != -1){
                index++;
            }
            else{
                pointer[index] = data_block_alloc();
                adicionado = true;
                blocks_to_write++;
            }
        }
    }
    return blocks_to_write;
}


ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    size_t blocks_to_write = 0, target_block, written = 0, bytes_to_write = 0,  not_written;
    if (file == NULL) {
        return -1;
    }
    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    pthread_rwlock_wrlock( &inode->lock);
    pthread_rwlock_wrlock( &file->lock);
    if (inode == NULL) {
        pthread_rwlock_unlock( &inode->lock);
        pthread_rwlock_unlock( &file->lock);
        return -1;
    }
    /* Determine how many bytes to write */
    if (to_write + file->of_offset > (10 + (BLOCK_SIZE/sizeof(int))) * BLOCK_SIZE) {
        to_write =(10 + (BLOCK_SIZE/sizeof(int)))*BLOCK_SIZE - file->of_offset;
    }
    if (to_write > 0){
        if((to_write)/BLOCK_SIZE > 0){
            blocks_to_write = calculate_more_block(inode, to_write);
        }
        else{
            blocks_to_write = calculate_less_block(inode, to_write);
        }
        not_written = to_write;
        while(blocks_to_write != 0 && not_written != 0){
            target_block = (file->of_offset)/BLOCK_SIZE;
            void *block;
            if(target_block < 10){
                block = data_block_get(inode->i_data_block[target_block]);
            }
            else{
                int* pointer = (int*) data_block_get(inode->i_data_block[10]);
                block = data_block_get(pointer[target_block -10]);
            }

            if (block == NULL) {
                pthread_rwlock_unlock( &inode->lock);
                pthread_rwlock_unlock( &file->lock);
                return -1;
            }

            /* Perform the actual write */
            if(not_written > (BLOCK_SIZE - (file->of_offset % BLOCK_SIZE))){
                bytes_to_write = BLOCK_SIZE - (file->of_offset)%BLOCK_SIZE;
            }
            else{
                bytes_to_write = not_written;
            }
            pthread_rwlock_wrlock(&data_blocks_lock);
            memcpy(block + (file->of_offset%BLOCK_SIZE), buffer+written, bytes_to_write);
            pthread_rwlock_unlock(&data_blocks_lock);
            /* The offset associated with the file handle is
             * incremented accordingly */
            not_written = not_written - bytes_to_write;
            written += bytes_to_write;
            file->of_offset = file->of_offset + bytes_to_write;
            if (file->of_offset > inode->i_size) {
                inode->i_size = file->of_offset;
            }
            blocks_to_write--;
        }    
    }
    pthread_rwlock_unlock( &inode->lock);
    pthread_rwlock_unlock( &file->lock);
    return (ssize_t)written;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    size_t bytes_to_read, read=0;
    open_file_entry_t *file = get_open_file_entry(fhandle);
    void *block;
    int* pointer;
    if (file == NULL) {
        return -1;
    }
    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    pthread_rwlock_rdlock( &inode->lock);
    pthread_rwlock_wrlock( &file->lock);
    if (inode == NULL) {
        pthread_rwlock_unlock( &inode->lock);
        pthread_rwlock_unlock( &file->lock);
        return -1;
    }
    
    /* Determine how many bytes to read */
    size_t to_read = (inode->i_size - file->of_offset);
    if (to_read > len) {
        to_read = len;
    }
    
    size_t blocks_to_read = to_read / BLOCK_SIZE;
    if(to_read % BLOCK_SIZE != 0){
        blocks_to_read++;
        if( BLOCK_SIZE - (file->of_offset % BLOCK_SIZE) < to_read){
            blocks_to_read++;
        }
    }
    if (to_read > 0){
        while(blocks_to_read > 0){
            size_t block_to_read = (file->of_offset/BLOCK_SIZE);
            if(block_to_read < 10){
                block = data_block_get(inode->i_data_block[block_to_read]);
            }
            else{
                pointer = (int*) data_block_get(inode->i_data_block[10]);
                block = data_block_get(pointer[block_to_read - 10]);
            }
            if (block == NULL) {
                pthread_rwlock_unlock( &inode->lock);
                pthread_rwlock_unlock( &file->lock);
                return -1;
            }
            //ATENÇÃO!!!!!!!!!!!!!!!VER ESTA CONDIÇÃO!!!!!!!!!
            if((to_read + file->of_offset % BLOCK_SIZE) > BLOCK_SIZE){
                bytes_to_read = (BLOCK_SIZE - (file->of_offset % BLOCK_SIZE));
            }
            else{
                bytes_to_read = to_read;
            }
           pthread_rwlock_rdlock(&data_blocks_lock);
            /* Perform the actual read */
            memcpy(buffer + read, block + (file->of_offset % BLOCK_SIZE), bytes_to_read);
            pthread_rwlock_unlock(&data_blocks_lock);
            /* The offset associated with the file handle is
             * incremented accordingly */
            file->of_offset += bytes_to_read;
            read += bytes_to_read;
            to_read -= bytes_to_read;
            blocks_to_read--;
        }
    }
    pthread_rwlock_unlock( &inode->lock);
    pthread_rwlock_unlock( &file->lock);
    memcpy(buffer+read,"\0",1);
    return (ssize_t)read;
}


int tfs_copy_to_external_fs(char const *source_path, char const *dest_path){
    FILE* dest_file;
    if (tfs_open(source_path, 0) == -1){
        return -1;
    }
    int fhandle = tfs_open(source_path, 0);
    open_file_entry_t *file = get_open_file_entry(fhandle);
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }
    char buffer[inode->i_size];
    tfs_read(fhandle, buffer , sizeof(buffer));
    pthread_rwlock_rdlock( &inode->lock);
    pthread_rwlock_wrlock( &file->lock);
    dest_file = fopen(dest_path, "w+");
    if(dest_file == NULL){
        pthread_rwlock_unlock( &inode->lock);
        pthread_rwlock_unlock( &file->lock);
        return -1;
    }
    int i=0;
    while(buffer[i] !='\0'){
        fwrite(&buffer[i], sizeof(char), 1, dest_file);
        i++;
    }
    fclose(dest_file);
    pthread_rwlock_unlock( &inode->lock);
    pthread_rwlock_wrlock( &file->lock);
    return 0;
}
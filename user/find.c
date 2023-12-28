#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define MAX_PATH_LEN 512

void handle_dir_entry(char path[], int path_len, char* search_name, char* entry_name);
void find(char path[], int path_len, char* search_name, int dir_fd);

void handle_dir_entry(char path[], int path_len, char* search_name, char* entry_name){
    struct stat st;
    int fd;
    if ((fd = open(path, 0)) < 0){
        return;
    }
    if(fstat(fd, &st) < 0){
        close(fd);
        return;
    }
    switch(st.type){
    case T_DEVICE:
    case T_FILE:
        if (0 == strcmp(entry_name, search_name)){
            printf("%s\n", path);
        }
        break;
    case T_DIR:
        find(path, path_len, search_name, fd);
    break;
    }
    close(fd);
}

void find(char path[], int path_len, char* search_name, int dir_fd){
    struct dirent de;
    if (path_len + 1 + DIRSIZ + 1 > MAX_PATH_LEN * sizeof(char)){
        fprintf(2, "dir %s has reached max path leghnth\n", path);
    }
    while(read(dir_fd, &de, sizeof(de)) == sizeof(de)){
        if (0 == de.inum){
            continue;
        }
        if (memcmp(de.name, ".", 2) == 0 || memcmp(de.name, "..", 3) == 0){
            continue;
        }
        path[path_len] = '/';
        memmove(&path[path_len + 1], de.name, DIRSIZ);
        path[path_len + 1 + DIRSIZ] = '\0';
        handle_dir_entry(path, strlen(path), search_name, de.name); 
        path[path_len] = '\0';
    }
}


int main(int argc, char* argv[]){
    char* search_dir = 0;
    char* search_name = 0;
    if (argc != 2 && argc != 3){
        write(2, "wrong number of arguments\n", 26);
        exit(1);
    }
    if (2 == argc)
    {
        search_dir = ".";
        search_name = argv[1];
    }
    else 
    {
        search_dir = argv[1];
        search_name = argv[2];
    }
    int search_dir_len = strlen(search_dir);
    if (search_dir_len >= MAX_PATH_LEN)
    {
        write(2, "search dir path is to long\n", 27);
        exit(1);
    }
    char path_buf[MAX_PATH_LEN];
    strcpy(path_buf, search_dir);
    int fd;
    if ((fd = open(path_buf, 0)) < 0){
        fprintf(2, "can't open directory %s\n", path_buf);
        exit(1);
    }
    find(path_buf, search_dir_len, search_name, fd);
    exit(0);
}
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
main(int argc, char** argv){
    int pipe_from_parent_to_child[2];
    int pipe_from_child_to_parent[2];
    pipe(pipe_from_parent_to_child);
    pipe(pipe_from_child_to_parent);
    if (fork() == 0){ // child process
        int my_pid = getpid();
        close(0);
        dup(pipe_from_parent_to_child[0]);
        close(pipe_from_parent_to_child[0]);
        close(pipe_from_parent_to_child[1]);
        close(pipe_from_child_to_parent[0]);
        char byte_read[1];
        if (1 != read(0, byte_read, 1))
        {
            write(2, "read error in child\n", 20);
            exit(1);
        }
        fprintf(1, "%d: received ping\n", my_pid);
        close(1);
        dup(pipe_from_child_to_parent[1]);
        close(pipe_from_child_to_parent[1]);
        if (1 != write(1, byte_read, 1))
        {
            write(2, "write error in child\n", 21);
            exit(1);
        }
    } else 
    { // parent process
        int my_pid = getpid();
        close(pipe_from_child_to_parent[1]);
        close(pipe_from_parent_to_child[0]);
        if (1 != write(pipe_from_parent_to_child[1], "p", 1))
        {
            write(2, "write error in parent\n", 22);
            exit(1);
        }
        close(pipe_from_parent_to_child[1]);
        char byte_read[1];
        if (1 != read(pipe_from_child_to_parent[0], byte_read, 1))
        {
            write(2, "read error in parent\n", 21);
            exit(1);
        }
        if ('p' != byte_read[0])
        {
            write(2, "byte sent is not byte recived\n", 30);
            exit(1);
        }
        fprintf(1, "%d: received pong\n", my_pid);
    }
    exit(0);
}
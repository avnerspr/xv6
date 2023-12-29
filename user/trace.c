#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char* argv[]){
    if (argc < 2){
        fprintf(2, "trace: invalid argument count\n");
        exit(1);
    }
    if (fork() == 0)
    {
        trace(atoi(argv[1]));
        exec(argv[2], &argv[2]);
        trace(0);
    }
    wait((int *) 0);
    exit(0);
}
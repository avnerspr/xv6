#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_PRIME 35

/*Functions Declrations*/
// probably most of the funcs should be macros instead
void read_int(int fds_in, int* res);
void write_int(int fds_out, int num);
int pass_num(int fds_in, int fds_out, int my_prime);
void create_child(int fds[2]);
void do_child_process(int pipe_to_parent[2]);

/*Functions*/

void read_int(int fds_in, int* res){ // prob should be a macro
    int read_status = read(fds_in, res, sizeof(int));
    if (0 > read_status)
    {
        fprintf(2, "read error in process %d, process exits\n", getpid());
        exit(1);
    }
}
 
void write_int(int fds_out, int num){ // prob should be a macro
    if (sizeof(num) != write(fds_out, &num, sizeof(num)))
    {
        fprintf(2, "write error in process %d, process exits\n", getpid());
        exit(1);
    }
}

int pass_num(int fds_in, int fds_out, int my_prime){ // prob should be a macro
    int num_to_pass = 0;
    read_int(fds_in, &num_to_pass);
    if (num_to_pass % my_prime != 0){
        write_int(fds_out, num_to_pass);
    }
    return num_to_pass >= 0;
}

void create_child(int fds[2]){ // prob should be a macro
    if (pipe(fds) < 0){
        fprintf(2, "pipe opening error in process %d\n", getpid());
        exit(1);
    }
    if (0 == fork()){
        close(fds[1]);
        do_child_process(fds); // no return function
    }
    else {
        close(fds[0]);
    }
}

// no return function
void do_child_process(int pipe_to_parent[2]){
    
    // being born logic
    int prime_num = 0;
    read_int(pipe_to_parent[0], &prime_num);
    if (prime_num < 0){
        close(pipe_to_parent[0]);
        exit(0);
    }
    fprintf(1, "prime %d\n", prime_num);

    // create child logic
    int pipe_to_child[2];
    create_child(pipe_to_child);

    // pass numbers logic
    int to_continue = 0;
    do {
        to_continue = pass_num(pipe_to_parent[0], pipe_to_child[1], prime_num);
    } while(to_continue);
    close(pipe_to_parent[0]);
    close(pipe_to_child[1]);
    wait((int*) 0);
    exit(0);
}

int main(int argc, char *argv[]){

    int fds[2];
    create_child(fds);
    for(int i = 2; i <= MAX_PRIME; i++){
        write_int(fds[1], i);
    }
    write_int(fds[1], -1);
    close(fds[1]);
    wait((int*) 0);
    exit(0);
}
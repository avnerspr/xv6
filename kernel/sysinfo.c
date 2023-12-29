#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"


int sys_sysinfo(void){
    uint64 addr; // user pointer to struct sysinfo
    argaddr(0, &addr);
    struct sysinfo info;

    // get number of free processes
    info.nproc = used_proc_count();

    // get size of free memory in bytes
    info.freemem = count_free_memPages() * PGSIZE;

    // copy result to memory on process pagetable
    struct proc* p = myproc(); 
    if(copyout(p->pagetable, addr, (char *)&info, sizeof(struct sysinfo)) < 0)
        return -1;
    return 0;
}

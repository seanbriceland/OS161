/*
 * proc_syscalls.c
 * SPB & FAR
 * Process Syscall functions
 *   1) sys__exit
 * 	2) sys_waitpid
 * 	3) ksys_waitpid
 * 	4) sys_getpid
 * 	5) sys_execv
 * 	6) sys_fork
 *
 * 	Proc Sysceall Helper functions
 * 	1) enter_forked_process
 * 	2) process_init
 * 	3) add_process
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/wait.h>
#include <copyinout.h>
#include <uio.h>
#include <lib.h>
#include <spl.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <vnode.h>
#include <syscall.h>
#include <test.h>
#include <synch.h>
#include <kern/seek.h>
#include <stat.h>
#include "filetable.h"
#include "openfile.h"
#include "process.h"


struct process* ptable[MAX_RUNNING_PROCS];

/**
 * sys__exit
 * sets the exit code for exiting process, resets parent pid child processes
 */
int sys__exit(int u_exitcode, int *retv)
{
	lock_acquire(ptable[curthread->pid]->lk_proc);

	u_exitcode = _MKWAIT_EXIT(u_exitcode);
	ptable[curthread->pid]->exitcode = u_exitcode;
	ptable[curthread->pid]->exited = 1;//Sets the exit flag
	for(int pid=0; pid < MAX_RUNNING_PROCS; pid++){
		if(ptable[pid] == NULL){
			//Intentionally SKIP
		}
		else if( ptable[pid]->parent_pid == curthread->pid ){
			ptable[pid]->parent_pid = -1;//Sets PPID to invalid number
		}
	}

	*retv = 0;
	cv_broadcast(ptable[curthread->pid]->waitcv, ptable[curthread->pid]->lk_proc);
	lock_release(ptable[curthread->pid]->lk_proc);
	thread_exit();
	return 0;
}

/**
 * sys_waitpid
 */
int sys_waitpid(int pid, int *status, int options, int *retv){
    (void)options;
    int err;
    //Make sure pid is a valid pid
    if(pid >= MAX_RUNNING_PROCS || pid < 0 || ptable[pid]==NULL ){
        *retv = -1;
        return ESRCH;
    }

    if(status == NULL){
        *retv = -1;
        return EFAULT;
    }

    //Check STATUS points to KERNEL SPACE
    if ((vaddr_t)status >= (vaddr_t)USERSPACETOP) {/* region is within the kernel */
        *retv = -1;
        return EFAULT;
    }

    if((vaddr_t)status % 4 != 0){//*status%4 != 0 ||
        *retv = -1;
        return EFAULT;
    }

    if (options != 0 ){
        *retv = -1;
        return EINVAL;
    }

    if(curthread->pid != ptable[pid]->parent_pid){
        *retv = -1;
        return ECHILD;
    }

    if(ptable[pid]->exited == 0){
        lock_acquire(ptable[pid]->lk_proc);
        cv_wait(ptable[pid]->waitcv, ptable[pid]->lk_proc);
    }

    //Signalled from child exiting
    *retv = pid;

    err = copyout((const void*)&(ptable[pid]->exitcode), (userptr_t) status, sizeof(ptable[pid]->exitcode));
    if (err){
        *retv = -1;
        return err;
    }

    lock_release(ptable[pid]->lk_proc);

    cv_destroy(ptable[pid]->waitcv);
    lock_destroy(ptable[pid]->lk_proc);
    kfree(ptable[pid]);
    ptable[pid] = NULL;
    return 0;
}

/**
 * ksys_waitpit
 * Used to wait pid's when called from kernel space
 */
int ksys_waitpid(int pid, int *status, int options, int *retv){
	if(pid >= MAX_RUNNING_PROCS || pid < 0 || ptable[pid]==NULL ){
	        *retv = -1;
	        return ESRCH;
	}
    if (options != 0 ){//|| options != WNOHANG || options != WUNTRACED){
        *retv = -1;
        return EINVAL;
    }

    if(curthread->pid != ptable[pid]->parent_pid){
        *retv = -1;
        return ECHILD;
    }

    if(ptable[pid]->exited == 0){
        lock_acquire(ptable[pid]->lk_proc);
        cv_wait(ptable[pid]->waitcv, ptable[pid]->lk_proc);
    }

    //Signalled from child exiting
    *retv = pid;

    status = (int*) ptable[pid]->exitcode;
    lock_release(ptable[pid]->lk_proc);
    cv_destroy(ptable[pid]->waitcv);
    lock_destroy(ptable[pid]->lk_proc);
    kfree(ptable[pid]);
    ptable[pid] = NULL;
    return 0;

}
/**
 * sys_getpid
 * returns pid for curthread
 */
int sys_getpid(int *retv){
	*retv = curthread->pid;
	return 0;
}

/**
 * sys_execv
 */
int sys_execv(const char* argc, char** argv, int *retv)
{
    int err=0;
    size_t gotv;

    char** kbuf = (char**)kmalloc(128*sizeof(char*));

    int i = 0;
    while(1){//copy in pointers
    	err = copyin((userptr_t)&(argv[i]), &(kbuf[i]), sizeof(char*));
        if(err)
            return err;
        if(kbuf[i] == NULL)
            break;
        i++;
    }

    int argn = i;
    //total number of padded indexes needed
    //aligned by 4 for pointers
    int padLen = (argn + 1)*4;
    char* kargv[128];
    for (int i = 0; i < argn; i++){
        kargv[i] = (char*)kmalloc(PATH_MAX);
        bzero(kargv[i], PATH_MAX);
        err = copyinstr((userptr_t)kbuf[i], kargv[i], PATH_MAX, &gotv);
        if (err)
            return err;

        int t = 0;
        int modint = (strlen(kargv[i])+1)%4;//find remainder to allow for padding
        if(modint != 0)
            modint = 4 - modint;//padding finalized to align by 4

        // each arg string will be:
        // 1 for default null terminator +
        // size of actually argument +
        // padding necessary to align by 4
        t = strlen(kargv[i]) + 1 + modint;
        padLen = padLen + t;
    }


    char kargvp[padLen];
    bzero(kargvp, padLen);

    int off = (argn+1)*4;
    for(int i = 0; i<argn+1;i++){
        if(i == argn){//copy null pointer to last pointer
            ((char**)kargvp)[i] = NULL;
        }else{ // strcopy argument from its offset location
            ((char**)kargvp)[i] = (char*)off;
            strcpy(&(kargvp[off]), kargv[i]);

            int len = strlen(kargv[i])+1;
            if (len % 4 != 0) {
                len += 4 - (len % 4);
            }
            off += len;//adjust offset
        }
    }

    char progname[PATH_MAX];
    size_t got;

    err = copyinstr((userptr_t)argc, progname, PATH_MAX,&got);
    if(err != 0)
        return err;

    //handles empty progname error
    if(got == 1)
        return EINVAL;

    struct vnode *v;

    /* Open the executable. */
    err = vfs_open(progname, O_RDONLY, 0, &v);
    if (err)
        return err;

    /* Create a new address space. */
    struct addrspace* new_as = as_create();
    if (new_as == NULL) {
        vfs_close(v);
        return ENOMEM;
    }

    /* Activate it. */
    as_activate(new_as);
    struct addrspace* old_as = curthread->t_addrspace;
    curthread->t_addrspace = new_as;

    /* Load the executable. */
    vaddr_t entrypoint, stackptr;
    err = load_elf(v, &entrypoint);
    if (err) {
        vfs_close(v);
        curthread->t_addrspace = old_as;
        return err;
    }
    as_destroy(old_as);

    // COPY ARGUMENTS INTO USER STACK
    /* Define the user stack in the address space */
    err = as_define_stack(curthread->t_addrspace, &stackptr);
    if (err) {
        return err;
    }
    stackptr -= padLen;

    for (int i = 0; i < argn; i++) {
        ((char**)kargvp)[i] += stackptr;
    }

    err = copyout(kargvp, (userptr_t)stackptr, padLen);
    if(err)
        return err;

    curthread->t_name = kstrdup(progname); //reset name

    vfs_close(v);/* Definitely Done with the file now.*/

    *retv = 0;

    /* Warp to user mode. */
    enter_new_process(/*progname*/argn, (userptr_t)stackptr,
              stackptr, entrypoint);

    // WE SOULD NEVER RETURN from Uspace!
    panic("enter_new_process returned\n");
    return 0;
}

/**
 * sys_fork
 * ON SUCCESS: fork returns twice,
 * 		once in the parent process
 * 			- In the parent process, the process id of the new child process is returned.
 * 		once in the child process.
 * 			- In the child process, 0 is returned.
 * ON ERROR: no new process is created, fork only returns once, returning -1, and errno is set according to the error encountered.
 */
int sys_fork(struct trapframe* ptf, int *retv)
{
	int spl = splhigh();//disable interrupts to prevent stale address space copying

	int err = 0;
	struct thread *child;// To be used to copy FT and AS
	struct trapframe* ctf = (struct trapframe*)kmalloc(sizeof(struct trapframe));
	if (ctf ==NULL){
		splx(spl);
		return ENOMEM;
	}

	*ctf = *ptf;

	struct addrspace* cas;
	err = as_copy(curthread->t_addrspace, &cas);
	if(err){
		splx(spl);
		return err;
	}

	err = thread_fork(curthread->t_name, enter_forked_process, ctf, (unsigned long) cas, &child);
	if (err){
		splx(spl);
		return err;
	}

	//"copy" parents filetable to the childs & add references where !NULL
	for(int fd = 0; fd<OPEN_MAX; fd++){
		child->filetable[fd] = curthread->filetable[fd];
		if(child->filetable[fd] != NULL)
			child->filetable[fd]->refcount++;
	}

	ptable[child->pid]->parent_pid = curthread->pid;//Sets the ppid for the child
	*retv = (int) child->pid;
	splx(spl);//Re-Enable interrupts after thread_fork

	return 0;
}

/**
 * enter_forked_process
 * Enter user mode for a newly forked process.
 * This will be our ENTRYPOINT function for thread_forking a new process.
 */
void
enter_forked_process(void* data1, unsigned long ul)//, unsigned long ul)
{
	struct trapframe *tf = (struct trapframe*) data1;
	struct addrspace *as = (struct addrspace*)ul;

	struct trapframe stack_tf;
	stack_tf = *tf;
	stack_tf.tf_a3 = 0;
	stack_tf.tf_v0 = 0;
	stack_tf.tf_epc += 4;

	as_copy(as, &curthread->t_addrspace);
	as_activate(curthread->t_addrspace);//activate the AS

	//warp to user mode with our stack tf
	mips_usermode(&stack_tf); //Enter user mode for newly forked process
}

//////////////////////////////////
//HELPER FUNCTIONS

/**
 * process_init
 * Initializes a process to be placed into our process table
 * allocates memory, sets up fields, calls add_process.
 * This should be called from fork().
 */
 pid_t process_init(struct thread* t){
     struct process *proc = kmalloc(sizeof(struct process));

     if(proc == NULL)
    	 return -1;//fails - handled by caller

     pid_t mypid;

     if(ptable[2] == NULL){
         proc->parent_pid = -5;//curthread->pid;//INIITS the default pid to negative for testing
     }else{
         proc->parent_pid = curthread->pid;
     }
     proc->exited = 0;
     proc->exitcode = -1;// means it has not been set and thus not exited
     proc->self = t;
     proc->waitcv = cv_create("process_cv");
     proc->lk_proc = lock_create("process_lk");
     mypid = add_process(proc);
     if(mypid < 0)
         return -1; //error handled by caller

     return mypid;
 }

/**
 * add_process
 * Adds process to the global process array.  This is called from process_init, which is called from fork().
 */
pid_t add_process(struct process* proc){

    for(int i = 2; i<MAX_RUNNING_PROCS + 2; i++){
        if(ptable[i] == NULL){
            ptable[i] = proc;
            return i;
        }
    }
    return -1; //error, full pt.
}

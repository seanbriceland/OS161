/**
 * SPB & FAR
 * Header File for process helper functions
 * global process table 
 * process struct
 */
#ifndef _PROCESS_H_
#define _PROCESS_H_

#define MAX_RUNNING_PROCS 256//TODO may need change this value.

//global array for holding all active processes.
extern struct process* ptable[];

//Process structure
struct process {
  pid_t parent_pid;
	struct cv *waitcv;//still not sure what this will be used for
	struct lock *lk_proc;
	int exited;
	int exitcode;
	struct thread* self;
};

//Function that will malloc the struct and set it's fields accordingly.
int process_init(struct thread* t); //allocates memory, sets up fields, calls add_process.
pid_t add_process(struct process* proc); //adds the process to the global process table.
#endif /* _PROCESS_H_ */ 


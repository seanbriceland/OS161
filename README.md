OS161
=====

Operating Systems Concepts

Problems and Implementations:
Implement system calls and exception handling. The full range of system calls that we think you might want over the course of the semester is listed in kern/include/kern/syscall.h. For this assignment you should implement:

File system support: open, read, write, lseek, close, dup2, chdir, and __getcwd.
Process support: getpid, fork, execv, waitpid, and _exit.
It's crucial that your syscalls handle all error conditions gracefully without crashing your kernel. You should consult the OS/161 man pages included in the distribution under man/syscall and understand fully the system calls that you must implement. You must return the error codes as decribed in the man pages.

Additionally, your syscalls must return the correct value—in case of success—or error code—in case of failure—as specified in the man pages. The grading scripts rely on the return of appropriate error codes and so adherence to the guidelines is as important as the correctness of your implementation.

The file user/include/unistd.h contains the user-level interface definition of the system calls that you will be writing for OS/161 (including ones you will implement in later assignments). This interface is different from that of the kernel functions that you will define to implement these calls. You need to design this interface and put it in kern/include/syscall.h. As you discovered in ASST0, the integer codes for the calls are defined in kern/include/kern/syscall.h.

You need to think about a variety of issues associated with implementing system calls. Perhaps the most obvious one: can two different user-level processes (or user-level threads, if you choose to implement them) find themselves running a system call at the same time? Be sure to argue for or against this, and explain your final decision in the executive summary and design document.

File System Support

For any given process, the first file descriptors (0, 1, and 2) are considered to be standard input (stdin), standard output (stdout), and standard error (stderr). These file descriptors should start out attached to the console device ("con:"), but your implementation must allow programs to use dup2() to change them to point elsewhere.

Although these system calls may seem to be tied to the filesystem, in fact, they are really about manipulation of file descriptors, or process-specific filesystem state. A large part of this assignment is designing and implementing a system to track this state. Some of this information—such as the current working directory—is specific only to the process, but others—such as file offset—is specific to the process and file descriptor. Don't rush this design. Think carefully about the state you need to maintain, how to organize it, and when and how it has to change.

Note that there is a system call __getcwd() and then a library routine getcwd(). Once you've written the system call, the library routine should function correctly.

Process Support

Process support for ASST2 divides into the easy (getpid()) and the not-so-easy: fork(), execv(), waitpid() and _exit()).

getpid()
A pid, or process ID, is a unique number that identifies a process. The implementation of getpid() is not terribly challenging, but process ID allocation and reclamation are the important concepts that you must implement. It is not OK for your system to crash because over the lifetime of its execution you've used up all the pids. Design your pid system, implement all the tasks associated with pid maintenance, and only then implement getpid().

fork(), execv(), waitpid() and _exit()
These system calls are probably the most difficult part of the assignment, but also the most rewarding. They enable multiprogramming and make OS/161 a usable system.

fork() is the mechanism for creating new processes. It should make a copy of the invoking process and make sure that the parent and child processes each observe the correct return value (that is, 0 for the child and the newly created pid for the parent). You will want to think carefully through the design of fork() and consider it together with execv() to make sure that each system call is performing the correct functionality. fork() is also likely to be a chance for you to use one of the syncronization primitives introduced in ASST1.

execv(), although merely a system call, is really the heart of this assignment. It is responsible for taking newly created processes and make them execute something different than what the parent is executing. It must replace the existing address space with a brand new one for the new executable—created by calling as_create in the current dumbvm system—and then run it. While this is similar to starting a process straight out of the kernel, as runprogram() does, it's not quite that simple. Remember that this call is coming out of userspace, into the kernel, and then returning back to userspace. You must manage the memory that travels across these boundaries very carefully. Also, notice that runprogram() doesn't take an argument vector, but this must of course be handled correctly by execv().

Although it may seem simple at first, waitpid() requires a fair bit of design. Read the specification carefully to understand the semantics, and consider these semantics from the ground up in your design. You may also wish to consult the UNIX man page, though keep in mind that you are not required to implement all the things UNIX waitpid() supports, nor is the UNIX parent/child model of waiting the only valid or viable possibility.

The implementation of _exit() is intimately connected to the implementation of waitpid(). They are essentially two halves of the same mechanism. Most of the time, the code for _exit() will be simple and the code for waitpid() relatively complicated, but it's perfectly viable to design it the other way around as well. If you find both are becoming extremely complicated, it may be a sign that you should rethink your design. waitpid()/_exit() is another chance to use synchronization primitives you designed for ASST1.

kill_curthread()
Feel free to write kill_curthread() in as simple a manner as possible. Just keep in mind that essentially nothing about the current thread's userspace state can be trusted if it has suffered a fatal exception. It must be taken off the processor in as judicious a manner as possible, but without returning execution to the user level.

Error Handling

The man pages in the OS/161 distribution contain a description of the error return values that you must return. If there are conditions that can happen that are not listed in the man page, return the most appropriate error code from kern/include/kern/errno.h. If none seem particularly appropriate, consider adding a new one. If you're adding an error code for a condition for which Unix has a standard error code symbol, use the same symbol if possible. If not, feel free to make up your own, but note that error codes should always begin with E, should not be EOF, etc. Consult Unix man pages to learn about Unix error codes. On Linux systems man errno will do the trick.

Note that if you add an error code to kern/include/kern/errno.h, you need to add a corresponding error message to the file kern/include/kern/errmsg.h.

Scheduling

Right now, the OS/161 scheduler implements a simple round-robin queue. You will see in thread.c that the schedule() function is actually blank. The round-robin scheduling comes into effect when hardclock() calls thread_yield() and a subsequent call to thread_switch() pops the current thread on the CPU's run-que and takes the next thread off the run-queue achieving FIFO ordering. As we discussed in class, this is probably not the best method for achieving optimal processor throughput.

For this assignment, you should design and implement a scheduling algorithm that gives priority to interactive threads, or those that seem interactive by sleeping frequently. You are free to implement any of the algorithms we have discussed in class or something different, and potentially simpler, as long as it achieves the goal.

We have tried to leverage the existing OS/161 configuration system to make it easy for you to switch between the default round-robin scheduler and your new implementation. To use the default scheduler:

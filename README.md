OS161
=====

Operating Systems Concepts

**Problems and Implementations:**

**SYNCH IMPLEMENTATIONS**

It's finally time to write some OS/161 code. The moment you've been waiting for!

It is possible to implement the primitives below on top of other primitives. It is, however, not always a great idea. You should definitely read and understanding the existing semaphore implementation since that can be used as a model for several of the other primitives we ask you to implement below.

Implement Locks

Implement locks for OS/161. The interface for the lock structure is defined in kern/include/synch.h. Stub code is provided in kern/threads/synch.c.

When you are done you should repeatedly pass the sy2 lock test provided by OS/161.

Note that you will not be able to run any of these tests an unlimited number of times. Due to limitations in the current virtual memory system used by your kernel, appropriately called dumbvm, your kernel is leaking a lot of memory. However, your synchronization primitives themselves should not leak memory, and you can inspect the kernel heap stats to ensure that they do not. (We will.)

you may wonder why, if the kernel is leaking memory, the kernel heap stats don't change between runs of sy1, for example, indicating that the semaphore implementation allocates and frees memory properly. The reason is that the kernel malloc() implementation we have provided is not broken, and it will correctly allocate, free and reallocate small items inside of the memory made available to it by the kernel. What does leak are larger allocations like, for example, the 4K thread kernel stacks, and it is these large items that eventually cause the kernel to run out of memory and panic(). Look at kern/arch/mips/vm/dumbvm.c for more details about what's broken and why.
Implement Condition Variables

Implement condition variables with Mesa—or non-blocking—semantics for OS/161. The interface for the condition variable structure is also defined in synch.h and stub code is provided in synch.c.

We have not discussed the differences between condition variable semantics. Two different varieties exist: Hoare, or blocking, and Mesa, or non-blocking. The difference is in how cv_signal is handled:

In Hoare semantics, the thread that calls cv_signal will block until the signaled thread (if any) runs and releases the lock.
In Mesa semantics the thread that calls cv_signal will awaken one thread waiting on the condition variable but will not block.
Please implement Mesa semantics. When you are done you should repeatedly pass the sy3 condition variable test provided by OS/161.

Implement Reader-Writer Locks

Implement reader-writer locks for OS/161. A reader-writer lock is a lock that threads can acquire in one of two ways: read mode or write mode. Read mode does not conflict with read mode, but read mode conflicts with write mode and write mode conflicts with write mode. The result is that many threads can acquire the lock in read mode, or one thread can acquire the lock in write mode. Your solution must also ensure that no thread waits to acquire the lock indefinitely, called starvation.

Unlike regular locks and condition variables, we have not provided you with an interface, stub code, or iron-clad semantics for reader-writer locks. You must develop your own. Your implementation must solve many readers, one writer problem and ensure that no writers are starved even in the presence of many readers. Build something you will be comfortable using later.

Implement your interface in synch.h and your code in synch.c. Please use the following self-explanatory templates for your code to ensure that we can run it from our test driver.

struct rwlock * rwlock_create(const char *);
void rwlock_destroy(struct rwlock *);

void rwlock_acquire_read(struct rwlock *);
void rwlock_release_read(struct rwlock *);
void rwlock_acquire_write(struct rwlock *);
void rwlock_release_write(struct rwlock *);
Unlike locks and condition variables, where we have provided you with a test suite, we are leaving it to you to develop a test that exercises your reader-writer locks. You will want to edit kern/startup/menu.c to allow yourself to run the command from the kernel menu or command line. We have our own reader-writer test that we will use to test and grade your implementation.

**SYNCH PROBLEMS**

The Classic CS161 Whale Mating Problem

eventually there will be "classic" CSE421 synchronization problems, but for this year we're giving credit where credit is due.
You have been hired by the New England Aquarium's research division to help find a way to increase the whale population. Because there are not enough of them, the whales are having synchronization problems in finding a mate. The trick is that in order to have children, three whales are needed; one male, one female, and one to play matchmaker—literally, to push the other two whales together.

Your job is to write the three procedures male(), female(), and matchmaker(). Each whale is represented by a separate thread. A male whale calls male(), which waits until there is a waiting female and matchmaker; similarly, a female whale must wait until a male whale and matchmaker are present. Once all three are present, the magic happens and then all three return.

Each whale thread should call the appropriate _start() function when it begins mating or matchmaking and the appropriate _end() function when mating or matchmaking completes. These functions are part of the problem driver in drivers.c and you are welcome to change them, but we will use our own versions for testing.

The test driver in drivers.c forks thirty threads, and has ten of them invoke male(), ten of them invoke female(), and ten of them invoke matchmaker(). Stub routines, which do nothing but call the appropriate _start() and _end() functions, are provided for these three functions. Your job will be to re-implement these functions so that they solve the synchronization problem described above.

When you are finished, you should be able to examine the output from running sp1 and convince yourself that your solution satisfies the constraints outlined above.

The Buffalo Intersection Problem

If you drive in Buffalo you know two things very well:

Four-way stops are very common.
Knowledge of how to correctly proceed through a four-way stop is very uncommon.
In general, four-way stops are so tricky that they've even been known to flummox the otherwise unflummoxable Google self-driving car, which both knows and is programmed to follow the rules.

Given that robot cars are the future anyway, we can rethink the entire idea of a four-way stop. Let's model the intersection as shown below. We consider the intersection as composed of four quadrants, numbered 0–3. Cars approach the intersection from one of four directions, also numbered 0–3. Note that we have numbered the quadrants so that a car approaching from direction X enters the intersection in quadrant X.

Given our model of the intersection, your job is to use synchronization primitives to implement a solution meeting the following two requirements:

No two cars may be in the same quadrant of the intersection at the same time. This constitutes a crash.
Once a car enters any intersection quadrant it must always be in some quadrant until it calls leaveIntersection().
Cars do not move diagonally between intersection quadrants.
Your solution should improve traffic flow compared to a conventional four-way stop while not starving traffic from any direction.

**SYSTEM CALLS**

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

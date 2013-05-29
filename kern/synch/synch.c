/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *  The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
// Semaphore.

struct semaphore *
sem_create(const char *name, int initial_count)
{
        struct semaphore *sem;

        KASSERT(initial_count >= 0);

        sem = kmalloc(sizeof(struct semaphore));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void 
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&sem->sem_lock);
  while (sem->sem_count == 0) {
		  wchan_lock(sem->sem_wchan);
	  	spinlock_release(&sem->sem_lock);
      wchan_sleep(sem->sem_wchan);

		  spinlock_acquire(&sem->sem_lock);
  }
  KASSERT(sem->sem_count > 0);
  sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
  KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

  sem->sem_count++;
  KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
// Lock - SPB & FAR

struct lock *
lock_create(const char *name)
{
    struct lock *lock;

    lock = kmalloc(sizeof(struct lock));
    if (lock == NULL) {
      return NULL;
    }

    lock->lk_name = kstrdup(name);
    if (lock->lk_name == NULL) {
      kfree(lock);
      return NULL;
    }

	lock->lk_wchan = wchan_create(lock->lk_name);
  if (lock->lk_wchan == NULL){
		kfree(lock->lk_name);
		kfree(lock);
	}

	lock->lk_owner = NULL;
	spinlock_init(&lock->lk_spinlock);
        
  return lock;
}

void
lock_destroy(struct lock *lock)
{
  KASSERT(lock != NULL);

	spinlock_cleanup(&lock->lk_spinlock);
	wchan_destroy(lock->lk_wchan);
  kfree(lock->lk_owner);
  kfree(lock->lk_name);
  kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	spinlock_acquire(&lock->lk_spinlock);
	if(lock_do_i_hold(lock))//we don't already have acquired it!
	{
		spinlock_release(&lock->lk_spinlock);
		return;
	}
	else
	{
		while(lock->lk_owner != NULL)
		{
			wchan_lock(lock->lk_wchan);
			spinlock_release(&lock->lk_spinlock);
			wchan_sleep(lock->lk_wchan);

			//returns from sleep here
			spinlock_acquire(&lock->lk_spinlock);
		}
		lock->lk_owner = curthread;
	}
	spinlock_release(&lock->lk_spinlock);
}

void
lock_release(struct lock *lock)
{
	spinlock_acquire(&lock->lk_spinlock);
  if(lock_do_i_hold(lock))
	{
		lock->lk_owner = NULL;
		wchan_wakeone(lock->lk_wchan);
	}
	spinlock_release(&lock->lk_spinlock);
}

bool
lock_do_i_hold(struct lock *lock)
{
	return lock->lk_owner == curthread;
}

////////////////////////////////////////////////////////////
// CV - SPB & FAR


struct cv *
cv_create(const char *name)
{
  struct cv *cv;

  cv = kmalloc(sizeof(struct cv));
  if (cv == NULL) {
    return NULL;
  }

  cv->cv_name = kstrdup(name);
  if (cv->cv_name==NULL) {
    kfree(cv);
    return NULL;
  }

  //create the wchan for CV
  cv->cv_wchan = wchan_create(cv->cv_name);
  if (cv->cv_wchan == NULL)
  {
  	kfree(cv->cv_name);
  	kfree(cv);
  }
      
  return cv;
}

void
cv_destroy(struct cv *cv)
{
  KASSERT(cv != NULL);

  wchan_destroy(cv->cv_wchan);
  kfree(cv->cv_name);
  kfree(cv);
}


void
cv_wait(struct cv *cv, struct lock *lock)
{
	KASSERT( cv != NULL );
	wchan_lock(cv->cv_wchan);
	lock_release(lock);
	wchan_sleep(cv->cv_wchan);
	lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	KASSERT( lock_do_i_hold(lock) );
	wchan_wakeone(cv->cv_wchan);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	KASSERT( lock_do_i_hold(lock) );
	wchan_wakeall(cv->cv_wchan);
}

////////////////////////////////////////////////////////////
// RW Lock - SPB & FAR

struct rwlock * rwlock_create(const char *name)
{
  struct rwlock *rwlock;
  rwlock = kmalloc(sizeof(struct rwlock));
  if (rwlock == NULL) {
    return NULL;
  }

  rwlock->rwlock_name = kstrdup(name);
  if (rwlock->rwlock_name == NULL) {
    kfree(rwlock);
    return NULL;
  }

  rwlock->write_cv = cv_create(rwlock->rwlock_name);
  if (rwlock->write_cv == NULL)
	{
		cv_destroy(rwlock->write_cv);
		return NULL;
	}

  rwlock->read_cv = cv_create(rwlock->rwlock_name);
  if (rwlock->read_cv == NULL)
  {
    cv_destroy(rwlock->read_cv);
    return NULL;
  }

  rwlock->lk = lock_create(rwlock->rwlock_name);
  if (rwlock->lk == NULL)
  {
    lock_destroy(rwlock->lk);
    return NULL;
  }
  rwlock->hold_readers = 0;
  rwlock->is_writing = 0;
  rwlock->num_readers = 0;

  return rwlock;
}

void rwlock_destroy(struct rwlock *rwlock)
{
    KASSERT(rwlock != NULL);

    lock_destroy(rwlock->lk);
    cv_destroy(rwlock->write_cv);
    cv_destroy(rwlock->read_cv);

    kfree(&rwlock->hold_readers);
    kfree(&rwlock->is_writing);
    kfree(&rwlock->num_readers);
    kfree(rwlock->rwlock_name);
    kfree(rwlock);
}

void rwlock_acquire_read(struct rwlock *rwlock)
{
	lock_acquire(rwlock->lk);

	//while a writer is writing DONT LET ANY READERS IN
	//OR if we need to hold readers - aka LET CURRENT READERS FINISH!
	while(rwlock->is_writing == 1 || rwlock->hold_readers == 1){
		cv_wait(rwlock->read_cv, rwlock->lk);//READERS THAT ARE WAITING FOR WRITERS
	}
	rwlock->num_readers++;//current reader reading!
	lock_release(rwlock->lk);
}

void rwlock_release_read(struct rwlock *rwlock)
{
	lock_acquire(rwlock->lk);
	rwlock->num_readers--;//Current reader DONE reading.

	//After each reader releases the lock, we want to see if writers are waiting
	//If so we want to let all readers to finish which would allow a writer to come in
	//Else if there are no writers waiting we can keep signaling readers if they are there.
	if(rwlock->hold_readers == 1 && rwlock->num_readers > 0){
		//still readers that need to finish - so do nothing until they finish.
	}	//once all readers have finished:
	else if(rwlock->hold_readers == 1 && rwlock->num_readers == 0){
		//SIGNAL READERS AND WRITERS AND LET THEM COMPETE evenly
		rwlock->hold_readers = 0;//we don't need to hold readers anymore
    //HERE WE ARE PURPOSELY BROADCASTING READERS AND NOT WRITERS
    //Since we expect their to be mmore read instructions than write
		cv_signal(rwlock->read_cv, rwlock->lk);
		cv_signal(rwlock->write_cv, rwlock->lk);
	}else{	//No writers waiting so broadcast all readers if any waiting
		cv_broadcast(rwlock->read_cv, rwlock->lk);
	}
	lock_release(rwlock->lk);
}

void rwlock_acquire_write(struct rwlock *rwlock)
{
	lock_acquire(rwlock->lk);

	//IF another thread is CURRENTLY reading or writing, allow it to finish
	while(rwlock->is_writing == 1 || rwlock->num_readers > 0){
		rwlock->hold_readers = 1;
		cv_wait(rwlock->write_cv, rwlock->lk);
	}
	rwlock->hold_readers = 0;//readers finished,since we are now writing
	rwlock->is_writing = 1;//writer has the lock and is writing
	lock_release(rwlock->lk); //--Don't release lock till writing is ready to release??--
}

void rwlock_release_write(struct rwlock *rwlock)
{
	lock_acquire(rwlock->lk);//--writer finished writing... now releasing--
	rwlock->is_writing = 0;//writers should go back to 0

	//After writer releases, lock become "UNHELD" either a reader OR a writer can come next
	//Should we randomly allow either a read or write to come next??
	//SIGNAL ALL Readers and WRITERS NOW! - Since we have the lock, they will be competing
	//for the lock once we release it.
  //HERE WE ARE PURPOSELY BROADCASTING READERS AND NOT WRITERS 
  //Since we expect their to be mmore read instructions than write
	cv_signal(rwlock->read_cv, rwlock->lk);
	cv_signal(rwlock->write_cv, rwlock->lk);

	lock_release(rwlock->lk);
}

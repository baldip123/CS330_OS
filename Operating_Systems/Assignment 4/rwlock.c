#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<common.h>

/* XXX NOTE XXX  
       Do not declare any static/global variables. Answers deviating from this 
       requirement will not be graded.
*/
void init_rwlock(rwlock_t *lock)
{
   /*Your code for lock initialization*/
   lock->value=0x1000000000000;
}

void write_lock(rwlock_t *lock)
{
   /*Your code to acquire write lock*/
   long var=0x1000000000000;
	while(lock->value <= 0){
		sched_yield();
	}
	while(atomic_add(&(lock->value),-var) < 0){
		atomic_add(&(lock->value),var);
		while(lock->value <= 0){
         sched_yield();
		}
      // sched_yield();
	}
}

void write_unlock(rwlock_t *lock)
{
   /*Your code to release the write lock*/
   atomic_add(&(lock->value),0x1000000000000);
}


void read_lock(rwlock_t *lock)
{
   /*Your code to acquire read lock*/
   while(atomic_add(&(lock->value),-0X1) < 0){
		atomic_add(&(lock->value),0X1);
		sched_yield();
	}	
}

void read_unlock(rwlock_t *lock)
{
   /*Your code to release the read lock*/
   atomic_add(&(lock->value),1);
}

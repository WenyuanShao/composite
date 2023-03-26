#include <llprint.h>
#include <sched.h>
#include <sync_sem.h>

static struct sync_sem sem;
volatile int give = 0;
volatile int take = 0;
volatile unsigned long long it = 0;

void
cos_init()
{
	assert(NUM_CPU == 2);
	sync_sem_init(&sem, 0);
}

void
__spin()
{
	it = 0;
	while (it < 100000000) {
		it++;
	}
}

void
parallel_main(coreid_t cid, int init_core, int ncores)
{
	if (cid == 0) {
		while (1)
		{
			printc("G");
			sync_sem_give(&sem);
		}
		
	} else {
		while (1)
		{
			printc("T\n");
			sync_sem_take(&sem);
		}
	}
	return;
}

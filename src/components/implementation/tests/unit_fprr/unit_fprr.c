/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_defkernel_api.h>
#include <llprint.h>
#include <res_spec.h>
#include <sl.h>

/* Ensure this is the same as what is in sl_mod_fprr.c */
#define SL_FPRR_NPRIOS 32

#define LOWEST_PRIORITY (SL_FPRR_NPRIOS - 1)

#define LOW_PRIORITY (LOWEST_PRIORITY - 1)
#define HIGH_PRIORITY (LOWEST_PRIORITY - 10)

int lowest_was_scheduled = 0;

void
low_thread_fn()
{
	lowest_was_scheduled = 1;
	sl_thd_free(sl_thd_curr());
}

void
high_thread_fn()
{
	struct sl_thd *low_thread = sl_thd_alloc(low_thread_fn, NULL);
	sl_thd_param_set(low_thread, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	cycles_t deadline = sl_now() + sl_usec2cyc(10 * 1000 * 1000);
	while (sl_now() < deadline) {}
	assert(!lowest_was_scheduled);
	sl_thd_free(sl_thd_curr());
}

void
test_highest_is_scheduled(void)
{
	struct sl_thd *high_thread = sl_thd_alloc(high_thread_fn, NULL);
	sl_thd_param_set(high_thread, sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

	cycles_t wakeup = sl_now() + sl_usec2cyc(1000 * 1000);
	sl_thd_block_timeout(0, wakeup);
}

int thd1_ran = 0;
int thd2_ran = 0;

void
thd1_fn()
{
	thd1_ran = 1;
	while(1);
}

void
thd2_fn()
{
	thd2_ran = 1;
	while(1);
}

void
allocator_thread_fn()
{
	struct sl_thd *thd1 = sl_thd_alloc(thd1_fn, NULL);
	sl_thd_param_set(thd1, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	struct sl_thd *thd2 = sl_thd_alloc(thd2_fn, NULL);
	sl_thd_param_set(thd2, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	cycles_t wakeup = sl_now() + sl_usec2cyc(1000 * 1000);
	sl_thd_block_timeout(0, wakeup);

	sl_thd_free(thd1);
	sl_thd_free(thd2);

	sl_thd_free(sl_thd_curr());
}

void
test_swapping(void)
{
	struct sl_thd *allocator_thread = sl_thd_alloc(allocator_thread_fn, NULL);
	sl_thd_param_set(allocator_thread, sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

	cycles_t wakeup = sl_now() + sl_usec2cyc(100 * 1000);
	sl_thd_block_timeout(0, wakeup);
}

void
run_tests()
{
	test_highest_is_scheduled();
	printc("Test successful! Highest was scheduled only!\n");
	test_swapping();
	printc("Test successful! We swapped back and forth!\n");

	printc("Done testing, faulting...\n");
	assert(0);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);

	printc("Unit-test for the scheduling library (sl)\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	sl_init();

	struct sl_thd *testing_thread = sl_thd_alloc(run_tests, NULL);
	sl_thd_param_set(testing_thread, sched_param_pack(SCHEDP_PRIO, LOWEST_PRIORITY));

	sl_sched_loop();

	assert(0);

	return;
}

#include "printk.h"
#include "gdt.h"
#include "user.h"
#include "types.h"
#include "tss.h"
#include "shared/consts.h"

#define IA32_SYSENTER_CS 0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

extern uint32_t user_size;
extern uint32_t *base_user_address;

//#define BASE_ADDRESS base_user_address
#define BASE_ADDRESS SERVICE_START

static inline void
writemsr(uint32_t reg, uint32_t low, uint32_t high)
{
  __asm__("wrmsr" : : "c" (reg), "a"(low), "d"(high));
}

uint32_t user_mode_stack;

void
user__init(void)
{
  uint32_t user_mode_stack = (uint32_t)BASE_ADDRESS + user_size + MAX_STACK_SZ;

  writemsr(IA32_SYSENTER_CS, SEL_KCSEG, 0);
  writemsr(IA32_SYSENTER_ESP, (uint32_t)tss_get()->esp0, 0);
  writemsr(IA32_SYSENTER_EIP, (uint32_t)&sysenter, 0);

  printk (INFO, "SYSENTER Seg 0x%x, ESP 0x%x, EIP 0x%x\n", SEL_KCSEG, tss_get()->esp0, &sysenter);
  printk (INFO, "About to jump to 0x%x with stack at 0x%x\n", BASE_ADDRESS, user_mode_stack);

  __asm__("sysexit" : : "c" (user_mode_stack), "d"(BASE_ADDRESS));
}

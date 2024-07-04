/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file peripheral clock interface for Actions SoC
 */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include <kernel_structs.h>
#include <kallsyms.h>
#include <stack_backtrace.h>
#include "csky_backtrace.h"
#include <linker/linker-defs.h>

extern struct _k_thread_stack_element _interrupt_stack[];
extern NANO_ESF _default_esf;
extern int _arch_exception_vector_read(void);

#ifdef CONFIG_ENABLE_TRACE_EVTTOSTR
extern const char *task_evt2str(int num);
#endif

uint32_t csky_get_current_pc(void);
__asm__ (
	".text\n\t"
	".global csky_get_current_pc\n\t"
	"csky_get_current_pc:\n\t"
	"mov    r0, lr\n\t"
	"rts\n\t");

uint32_t csky_get_current_sp(void);
__asm__ (
	".text\n\t"
	".global csky_get_current_sp\n\t"
	"csky_get_current_sp:\n\t"
	"mov    r0, sp\n\t"
	"rts\n\t");

#ifndef CONFIG_KALLSYMS
static void sh_dump_mem(const char *msg, uint32_t mem_addr, int len)
{
	int i;
	uint32_t *ptr = (uint32_t *)mem_addr;
	printk("%s start=0x%08x end=0x%08x\n", msg, mem_addr, mem_addr + len);
	for(i = 0; i < len/4; i++)
	{
		printk("%08x ", ptr[i]);
		if(i % 8 == 7)
			printk("\n");
	}
	printk("\n\n");
}

static uint32_t _check_sp_end_addr(uint32_t sp_cur)
{
	struct k_thread *thread_list;

	if ((sp_cur >= (uint32_t)_interrupt_stack)
		&& (sp_cur <= (uint32_t)_interrupt_stack + CONFIG_ISR_STACK_SIZE)) {
		return (uint32_t)_interrupt_stack + CONFIG_ISR_STACK_SIZE;
	}

	thread_list = ((struct k_thread *)(_kernel.threads));
	while (thread_list != NULL) {
		if (sp_cur >= thread_list->stack_info.start && sp_cur <= (thread_list->stack_info.start + thread_list->stack_info.size)) {
			break;
		}

		thread_list = (struct k_thread *)(thread_list->next_thread);
	}

	if (!thread_list) {
		//Can not get stack area, maybe stack overflows.
		printk("stack error! overflow? SP=<%08x>\n", sp_cur);
		return sp_cur + 0x100;
	} else {
		return thread_list->stack_info.start + thread_list->stack_info.size;
	}
}

static void sh_backtrace_print_elx(uint32_t addr, uint32_t sp, uint32_t bak_lr, struct __esf *esf)
{
	uint32_t where = sp;
	uint32_t pc = 0;
	uint32_t stk_end = _check_sp_end_addr(sp);

	if(sp > stk_end){
		printk("stack overflow sp<%08x> end<%08x>\n", sp, stk_end);
		return;
	}

	printk("/opt/csky-elfabiv2/bin/csky-abiv2-elf-addr2line -e zephyr.elf -a -f ");
	if(esf){
		printk("%08x %08x ", esf->epc, esf->r15);
	}else{
		printk("%08x ", addr);
	}
	while (where < stk_end) {
		pc = *(uint32_t*)where;
		if( (pc > (uint32_t)&_image_rom_start) && (pc < (uint32_t)&_image_text_end) ){
			printk("%08x ", pc);
		}

		where += 4;
	}
	printk("\n");

	sh_dump_mem("stack:", sp, stk_end - sp);
	printk("\n");
}


#endif

static void __show_backtrace(struct k_thread *thread, struct __esf *esf)
{
	uint32_t addr;
	uint32_t sp;
	uint32_t bak_lr = 0;

	if (esf) {
		addr = esf->epc;

		if(esf->vec == 7){
			addr = esf->r15;
		}
		sp = esf->r14;
		/* first frame maybe doesn't have stack frame */
		bak_lr = esf->r15;
	} else {
		if (thread != NULL && thread != k_current_get()) {
			addr = thread->callee_saved.pc;
			sp = thread->callee_saved.sp;
		} else {
			addr = csky_get_current_pc();
			sp = csky_get_current_sp();
		}
	}

	TRACE_SYNC("Call Trace:\n");
#ifdef CONFIG_KALLSYMS
	csky_backtrace(addr, sp, bak_lr);
#else
	sh_backtrace_print_elx(addr, sp, bak_lr, esf);
#endif
}

void show_backtrace(struct k_thread *thread, struct __esf *esf)
{
    uint32_t param[4] = {
            (uint32_t) thread,
            (uint32_t) esf,
            (uint32_t) _interrupt_stack + CONFIG_ISR_STACK_SIZE,
            (uint32_t) __show_backtrace };

    uint32_t flags;

    flags = irq_lock();

	if ((uint32_t) &flags > (uint32_t) _interrupt_stack
			&& (uint32_t) &flags < \
			((uint32_t) _interrupt_stack + CONFIG_ISR_STACK_SIZE)) {

        __show_backtrace(thread, esf);
    }else{
        //backtrace maybe overflow in thread stack, we must switch it to interrupt stack
        //save the thread context,include sp, argument registers ...
		__asm__ __volatile (
				"	ldw	r0,	%0	\n"
				"	ldw	r1,	%1	\n"
				"	ldw	r2,	%2	\n"
				"	mov	r4,	r14	\n"
				"	mov	r14,r2	\n"
				"	ldw	r2,	%3	\n"
				"	jsr	r2		\n"
				"	mov	r14, r4	\n"
				:
				: "m" (param[0]), "m" (param[1]), "m" (param[2]), "m" (param[3])
				: "r0", "r1", "r2", "r4", "r14");
    }

    irq_unlock(flags);
}


unsigned int get_user_sp(void)
{
    uint32_t *sp_ptr = (uint32_t *)_interrupt_stack;

    return sp_ptr[CONFIG_ISR_STACK_SIZE / 4 - 2];
}


void show_thread_stack(struct k_thread *thread)
{
    struct __esf esf;

    struct irq_frame *frame;

    //if in isr, we must print irq stack and thread stack
    if(k_is_in_isr() && thread == k_current_get()){
        TRACE_SYNC("ISR: %d\n", _arch_irq_vector_read());

        show_backtrace(thread, NULL);

#ifdef CONFIG_ENABLE_TRACE_EVTTOSTR
	    TRACE_SYNC("Thread: %p %s %d %s\n", thread,
		thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread), task_evt2str(k_thread_priority_get(thread)));
#else
		TRACE_SYNC("Thread: %p %s %d\n", thread,
		thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread));
#endif

		//get the thread sp value from the top of interrupt stack
		frame = (struct irq_frame *)(_interrupt_stack + CONFIG_ISR_STACK_SIZE - sizeof(struct irq_frame));

        esf.r14 = get_user_sp();
        esf.epc = frame->epc;
        esf.r15 = frame->r15;
        esf.vec = 32;

		//TRACE_SYNC("irq frame pc %x\n", frame->epc);
		//TRACE_SYNC("irq frame ra %x\n", frame->r15);
		//TRACE_SYNC("irq frame sp %x\n", esf.r14);

		show_backtrace(thread, &esf);

    }else if((_arch_exception_vector_read() > 0) && (thread == k_current_get())){
        TRACE_SYNC("exception: %d\n", _arch_exception_vector_read());

        show_backtrace(thread, NULL);

#ifdef CONFIG_ENABLE_TRACE_EVTTOSTR
	    TRACE_SYNC("Thread: %p %s %d %s\n", thread,
		thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread), task_evt2str(k_thread_priority_get(thread)));
#else
		TRACE_SYNC("Thread: %p %s %d\n", thread,
		thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread));
#endif

        esf.r14 = _default_esf.r14;
        esf.epc = _default_esf.epc;
        esf.r15 = _default_esf.r15;
        esf.vec = _arch_exception_vector_read();

		//TRACE_SYNC("exc frame pc %x\n", frame->epc);
		//TRACE_SYNC("exc frame ra %x\n", frame->r15);
		//TRACE_SYNC("exc frame sp %x\n", esf.r14);

		show_backtrace(thread, &esf);
    }else{
#ifdef CONFIG_ENABLE_TRACE_EVTTOSTR
    	TRACE_SYNC("Thread: %p %s %d %s\n", thread,
    		thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread), task_evt2str(k_thread_priority_get(thread)));
#else
		TRACE_SYNC("Thread: %p %s %d\n", thread,
			thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread));
#endif
        if(thread == k_current_get()){
            esf.r14 = csky_get_current_sp();
            esf.epc = csky_get_current_pc();
            esf.r15 = 0;
            esf.vec = 0;
            show_backtrace(thread, &esf);
        }else{
            show_backtrace(thread, NULL);
        }
    }


}

void show_all_threads_stack(void)
{
#if defined(CONFIG_THREAD_MONITOR)
	struct k_thread *thread = NULL;

	thread = (struct k_thread *)(_kernel.threads);
	while (thread != NULL) {
		show_thread_stack(thread);
		thread = (struct k_thread *)thread->next_thread;
	}
#endif
}

void dump_stack(void)
{
	show_thread_stack(k_current_get());
}

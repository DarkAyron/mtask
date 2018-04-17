 /****************************************************************************
 *                                                                           *
 *                                                      ,8                   *
 *                                                    ,d8*                   *
 *                                                  ,d88**                   *
 *                                                ,d888`**                   *
 *                                             ,d888`****                    *
 *                                            ,d88`******                    *
 *                                          ,d88`*********                   *
 *                                         ,d8`************                  *
 *                                       ,d8****************                 *
 *                                     ,d88**************..d**`              *
 *                                   ,d88`*********..d8*`****                *
 *                                 ,d888`****..d8P*`********                 *
 *                         .     ,d8888*8888*`*************                  *
 *                       ,*     ,88888*8P*****************                   *
 *                     ,*      d888888*8b.****************                   *
 *                   ,P       dP  *888.*888b.**************                  *
 *                 ,8*        8    *888*8`**88888b.*********                 *
 *               ,dP                *88 8b.*******88b.******                 *
 *              d8`                  *8b 8b.***********8b.***                *
 *            ,d8`                    *8. 8b.**************88b.              *
 *           d8P                       88.*8b.***************                *
 *         ,88P                        *88**8b.************                  *
 *        d888*       .d88P            888***8b.*********                    *
 *       d8888b..d888888*              888****8b.*******        *            *
 *     ,888888888888888b.              888*****8b.*****         8            *
 *    ,8*;88888P*****788888888ba.      888******8b.****      * 8' *          *
 *   ,8;,8888*         `88888*         d88*******8b.***       *8*'           *
 *   )8e888*          ,88888be.       888*********8b.**       8'             *
 *  ,d888`           ,8888888***     d888**********88b.*    d8'              *
 * ,d88P`           ,8888888Pb.     d888`***********888b.  d8'               *
 * 888*            ,88888888**   .d8888*************      d8'                *
 * `88            ,888888888    .d88888b*********        d88'                *
 *               ,8888888888bd888888********             d88'                *
 *               d888888888888d888********                d88'               *
 *               8888888888888888b.****                    d88'              *
 *               88*. *88888888888b.*      .oo.             d888'            *
 *               `888b.`8888888888888b. .d8888P               d888'          *
 *                **88b.`*8888888888888888888888b...            d888'        *
 *                 *888b.`*8888888888P***7888888888888e.          d888'      *
 *                  88888b.`********.d8888b**  `88888P*            d888'     *
 *                  `888888b     .d88888888888**  `8888.            d888'    *
 *                   )888888.   d888888888888P      `8888888b.      d88888'  *
 *                  ,88888*    d88888888888**`        `8888b          d888'  *
 *                 ,8888*    .8888888888P`              `888b.        d888'  *
 *                ,888*      888888888b...                `88P88b.  d8888'   *
 *       .db.   ,d88*        88888888888888b                `88888888888     *
 *   ,d888888b.8888`         `*888888888888888888P`              ******      *
 *  /*****8888b**`              `***8888P*``8888`                            *
 *   /**88`                               /**88`                             *
 *   `|'                             `|*8888888'                             *
 *                                                                           *
 * mtask.c                                                                   *
 *                                                                           *
 * Do rudimentary multitasking.                                              *
 * This scheduler implements cooperative multitasking.                       *
 * Context switching occurs only on request.                                 *
 *                                                                           *
 * Copyright (c) 2017 Ayron                                                  *
 *                                                                           *
 *****************************************************************************
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation; either version 2 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License along  *
 *  with this program; if not, write to the Free Software Foundation, Inc.,  *
 *  51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.                  *
 *                                                                           *
 ****************************************************************************/

#include <string.h>
#include <cpu.h>

#ifdef SEGGER_DEBUG
#include <SEGGER/SEGGER_SYSVIEW.h>
#include <SEGGER/SEGGER_RTT.h>
#endif

#include "mtask.h"

extern volatile uint32_t jiffies;
extern void*traps;

#define SYS_SP (long)traps /* Stack pointer */
#define STACK_SIZE 0x400
#define MAX_THREADS 15

enum coroutineState {
	STATE_RUNNING,
	STATE_WAITING,
	STATE_TERMINATED
};

struct stackFrame {
	int R4;
	int R5;
	int R6;
	int R7;
	int R8;
	int R9;
	int R10;
	int R11;
	int R0;
	int R1;
	int R2;
	int R3;
	int R12;
	void *LR;
	void *PC;
	int xPSR;
};

struct coroutine {
	enum coroutineState state;
	enum triggers trigger;
	int context;
	void *stack;
	char name[16];
#ifdef FPU
	int magicLink;
#endif
};

struct coroutine coroutines[MAX_THREADS];
static struct {
	int cur;
	int next;
} coroutineNum;

static volatile uint32_t currentTick;
static volatile enum triggers currentTrigger;
static volatile int newInvoke;

/* bx lr with this value in lr causes what's known
 * as rti by other CPUs
 */
#define MAGIC_LINK 0xfffffff9

#ifdef SEGGER_DEBUG
/* for SEGGER SystemView */
void cbSendTaskList(void)
{
	int n;
	SEGGER_SYSVIEW_TASKINFO info;

	for (n = 1; n < MAX_THREADS; n++) {
		if (coroutines[n].state != STATE_TERMINATED) {
			SEGGER_SYSVIEW_OnTaskCreate((unsigned)&coroutines[n]);
			info.TaskID = (unsigned)&coroutines[n];
			info.sName = coroutines[n].name;
			info.StackBase = (uint32_t) coroutines[n].state;
			SEGGER_SYSVIEW_SendTaskInfo(&info);
		}
	}
}
#endif

/* idle routine. Just wait until current tick has finished */
static void coroutine_idle(unsigned long unused)
{
	int n;
	while (1) {
		/* reset trigger flag */
		currentTrigger = TRIGGER_NONE;
		while (currentTick == jiffies) {
			if (currentTrigger) {
				currentTrigger = TRIGGER_NONE;
				coroutine_yield(TRIGGER_NONE, 0);
			}
			if (newInvoke) {
				newInvoke = 0;
				coroutine_yield(TRIGGER_NONE, 0);
			}
			/*asm volatile ("wfi"); */
		}
		
		/* Tick is over, wake all coroutines */
		currentTick = jiffies;
		for (n = 0; n < MAX_THREADS; n++) {
			if ((coroutines[n].state == STATE_WAITING) && (coroutines[n].trigger == TRIGGER_NONE))
			coroutines[n].state = STATE_RUNNING;
		}
		coroutine_yield(TRIGGER_NONE, 0);
	}
}

/* Initialize the multitasking engine */
void mtask_init()
{
	int n;
	int curSP;
	char*newSP;

	for (n = 0; n < MAX_THREADS; n++) {
		coroutines[n].state = STATE_TERMINATED;
		coroutines[n].stack = (void *)(SYS_SP - n * STACK_SIZE);
		coroutines[n].trigger = TRIGGER_NONE;
#ifdef FPU
		coroutines[n].magicLink = MAGIC_LINK;
#endif
	}
	coroutineNum.cur = MAX_THREADS - 1;
	currentTick = 0;
	currentTrigger = TRIGGER_NONE;

	/*
	 * move the current stack to the last coroutine, to
	 * prevent fuxoring up the init stack.
	 */

	asm volatile ("mov %0, r13":"=l" (curSP));
	newSP = (char*)coroutines[MAX_THREADS - 1].stack;
	for (n = SYS_SP; n > curSP; n--) {
		*--newSP = *((char*)n - 1);
	}
	coroutines[MAX_THREADS - 1].stack = newSP;
	coroutines[MAX_THREADS - 1].state = STATE_RUNNING;

	asm volatile ("mov r13, %0\n\t"
			"mov r7, r13"::"l" (newSP));

	/* 
	 * set interrupt priorities.
	 * The lower the number, the higher the priority
	 */
	NVIC_SetPriority(SVCall_IRQn, 0x00);
	NVIC_SetPriority(PendSV_IRQn, 0x0f);
	NVIC_SetPriority(SysTick_IRQn, 0x0e);
	coroutine_invoke_later(coroutine_idle, 0, "Idler");

}

static int findNextCoroutine()
{
	int n;
	for (n = 0; n < MAX_THREADS; n++) {
		/* find next running coroutine */
		if (coroutines[n].state == STATE_RUNNING) {
			break;
		}
	}
	return n;
}

/* do a context switch */
__attribute__ ((naked))
void PendSV_Handler()
{
	register void *nextsp;
	register void *mySP;
	register int magicLink;
	__disable_irq();
#ifdef CORTEX_M0
	asm("push {r4 - r7}");
	asm("mov r4, r8");
	asm("mov r5, r9");
	asm("mov r6, r10");
	asm("mov r7, r11");
	asm("push {r4 - r7}");
#else
	asm("push {r4 - r11}");
#endif
	/* save the magic link */
#ifdef FPU
	asm("mov %r0, lr":"=l" (magicLink));
	coroutines[coroutineNum.cur].magicLink = magicLink;
#endif

	/* save current SP */
	asm volatile ("mov %r0, r13":"=l" (mySP));
	coroutines[coroutineNum.cur].stack = mySP;

	/* set stack pointer */
	nextsp = coroutines[coroutineNum.next].stack;
	asm volatile ("mov sp, %r0\n"::"l" (nextsp));

	coroutineNum.cur = coroutineNum.next;

	/* restore the magic link */
#ifdef FPU
	magicLink = coroutines[coroutineNum.cur].magicLink;
	asm("mov lr, %r0"::"l" (magicLink));
#endif

	/* restore the rest and trigger rti */
#ifdef CORTEX_M0
	asm("pop {r4 - r7}");
	asm("mov r8, r4");
	asm("mov r9, r5");
	asm("mov r10, r6");
	asm("mov r11, r7");
	asm("pop {r4 - r7}");
#else
	asm("pop {r4 - r11}");
#endif
	__enable_irq();
	asm("bx lr");
}

static void coroutine_switchToNext()
{
	__disable_irq();
	/* find the next coroutine */
	coroutineNum.next = findNextCoroutine();
	if (coroutineNum.next == MAX_THREADS) {
		coroutineNum.next = 0;
	}

	/* schedule a PendSV interrupt and wait until it happens */
#ifdef SEGGER_DEBUG
	if (coroutineNum.next)
		SEGGER_SYSVIEW_OnTaskStartExec((unsigned)
					       &coroutines[coroutineNum.next]);
	else
		SEGGER_SYSVIEW_OnIdle();
#endif

	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	__enable_irq();
}

/* we don't need to save anything here,
 * because we are in a destroyed stack frame
 */
__attribute__ ((naked))
void SVC_Handler()
{
	register void *nextsp;
	/* save the magic link */
	asm volatile ("mov r10, lr");

	/* mark the slot as free */
	coroutines[coroutineNum.cur].state = STATE_TERMINATED;

#ifdef SEGGER_DEBUG
	SEGGER_SYSVIEW_OnTaskStopExec();
#endif
	coroutine_switchToNext();
	/* reset the PendSV, as we are doing the switch here now */
	SCB->ICSR &= ~SCB_ICSR_PENDSVSET_Msk;

	coroutineNum.cur = coroutineNum.next;

	/* set stack pointer */
	nextsp = coroutines[coroutineNum.next].stack;
	asm volatile ("mov sp, %r0\n"::"l" (nextsp));

	/* restore the magic link */
	asm("mov lr, r10");

	/* restore the rest and trigger rti */
#ifdef CORTEX_M0
	asm("pop {r4 - r7}");
	asm("mov r8, r4");
	asm("mov r9, r5");
	asm("mov r10, r6");
	asm("mov r11, r7");
	asm("pop {r4 - r7}");
#else
	asm("pop {r4 - r11}");
#endif
	asm("bx lr");
}

static void coroutine_end()
{
	/* use supervisor call to block other interrupts while terminating the coroutine */
	coroutine_abort();
	/* there's nothing more to do */
	while (1) ;
}

static void coroutine_start(void (*func) (unsigned long), unsigned long param, int n, const char *name)
{
	struct stackFrame *frame;
#ifdef SEGGER_DEBUG
	SEGGER_SYSVIEW_TASKINFO info;
#endif

	coroutines[n].stack =
	    (void *)(SYS_SP - n * STACK_SIZE - sizeof(struct stackFrame));
	coroutines[n].state = STATE_RUNNING;
	coroutines[n].trigger = TRIGGER_NONE;
#ifdef FPU
	coroutines[n].magicLink = MAGIC_LINK;
#endif
	frame = (struct stackFrame *)coroutines[n].stack;
	frame->PC = (void *)func;
	frame->LR = (void *)coroutine_end;
	frame->R0 = param;
	frame->xPSR = 0x01000000;	/* Thumb bit must be set */
	if (n) {
#ifdef SEGGER_DEBUG
		SEGGER_SYSVIEW_OnTaskCreate((unsigned)&coroutines[n]);
#endif
		strcpy(coroutines[n].name, name);
#ifdef SEGGER_DEBUG
		info.TaskID = (unsigned)&coroutines[n];
		info.sName = coroutines[n].name;
		info.StackBase = (uint32_t) frame;
		SEGGER_SYSVIEW_SendTaskInfo(&info);
#endif
	}
}

/* yield the current coroutine to give the CPU to other stuff */
enum triggers coroutine_yield(enum triggers trigger, int context)
{
	coroutines[coroutineNum.cur].state = STATE_WAITING;
	coroutines[coroutineNum.cur].trigger = trigger;
	coroutines[coroutineNum.cur].context = context;
#ifdef SEGGER_DEBUG
	if (coroutineNum.cur)
		SEGGER_SYSVIEW_OnTaskStopReady((unsigned)
					       &coroutines[coroutineNum.cur],
					       trigger);
#endif
	coroutine_switchToNext();
	return coroutines[coroutineNum.cur].trigger;
	asm volatile ("wfi");
}

/* set up a new coroutine-slot and invoke it eventually in the current or next tick */
int coroutine_invoke_later(void (*func) (unsigned long), unsigned long param, const char *name)
{
	int n;
	for (n = 0; n < MAX_THREADS; n++) {
		if (coroutines[n].state == STATE_TERMINATED)
			break;
	}
	if (n == MAX_THREADS)
		return -1;

	coroutine_start(func, param, n, name);
	newInvoke = 1;
	return n;
}

/* set up a new coroutine-slot and invoke it immediately, suspending the current one */
int coroutine_invoke_urgent(void (*func) (unsigned long), unsigned long param, const char *name)
{
	int result = coroutine_invoke_later(func, param, name);
	if (result >= 0) {
		coroutineNum.next = result;
#ifdef SEGGER_DEBUG
		SEGGER_SYSVIEW_OnTaskStartExec((unsigned)
					       &coroutines[coroutineNum.next]);
#endif
		/* schedule a PendSV interrupt */
		SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	}

	return result;
}

/* abort the current coroutine and free the slot */
__attribute__ ((__always_inline__)) inline void coroutine_abort()
{
	asm volatile ("svc 0");
}

/* trigger an event */
void coroutine_trigger(enum triggers trigger, int context)
{
	int n;
	for (n = 0; n < 15; n++) {
		if ((coroutines[n].state == STATE_WAITING) && (coroutines[n].trigger & trigger)) {
			if  ((coroutines[n].context == context) || (coroutines[n].context == 0)) {
				coroutines[n].state = STATE_RUNNING;
				coroutines[n].trigger = trigger;
			}
		}
	}
	currentTrigger = trigger;
}

/*
 * Trampoline OS
 *
 * This software is distributed under the Lesser GNU Public Licence
 *
 * Trampoline posix machine dependent stuffs
 *
 */

#include "tpl_machine.h"
#include "tpl_os_generated_configuration.h"
#include "tpl_os_internal_types.h"
#include "tpl_viper_interface.h"
#include "tpl_os_it_kernel.h"
#include "tpl_os.h"
#include "tpl_machine_interface.h"
#include "tpl_os_application_def.h" /* define NO_ISR if needed. */
#ifdef WITH_AUTOSAR_TIMING_PROTECTION
#include "tpl_as_timing_protec.h"
#endif /* WITH_AUTOSAR_TIMING_PROTECTION */
#ifdef WITH_AUTOSAR
#include "tpl_as_isr_kernel.h"
#include "tpl_os_kernel.h" /* for tpl_running_obj */
#include "tpl_as_definitions.h"
#include "tpl_os_task_kernel.h"
#include "tpl_os_rez_kernel.h"
#endif /* WITH_AUTOSAR */

#if defined(__unix__) || defined(__APPLE__)
	#include <assert.h>
#endif
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

VAR(tpl_stack_word, OS_VAR) idle_stack_zone[32768/sizeof(tpl_stack_word)] = {0} ;
VAR(struct TPL_STACK, OS_VAR) idle_task_stack = { idle_stack_zone, 32768} ;
VAR(struct TPL_CONTEXT, OS_VAR) idle_task_context;

extern volatile u32 tpl_locking_depth;
extern VAR(tpl_bool, OS_VAR) tpl_user_task_lock;
extern VAR(tpl_bool, OS_VAR) tpl_cpt_os_task_lock;

#ifdef WITH_AUTOSAR_TIMING_PROTECTION

FUNC(void, OS_CODE) tpl_watchdog_callback(void)
{
}

static struct timeval startup_time;

tpl_time tpl_get_local_current_date ()
{
    struct timeval time;
    tpl_time result;
  
    gettimeofday (&time, NULL);
    result = ((time.tv_sec - startup_time.tv_sec) % 2000) * (1000 * 1000) + 
        (time.tv_usec - startup_time.tv_usec);
  
    return result;
}

void tpl_set_watchdog (tpl_time delay)
{
    struct itimerval timer;
  
    /* configure and start the timer */
    timer.it_value.tv_sec = delay / (1000 * 1000);
    timer.it_value.tv_usec = delay % (1000 * 1000);
    timer.it_interval.tv_sec = delay / (1000 * 1000);
    timer.it_interval.tv_usec = delay % (1000 * 1000);
    setitimer (ITIMER_REAL, &timer, NULL);
}

void tpl_cancel_watchdog(void)
{
    struct itimerval timer;
  
    /* disable the timer */
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;
    setitimer (ITIMER_REAL, &timer, NULL);
}
#endif /* WITH_AUTOSAR_TIMING_PROTECTION */

#ifdef WITH_AUTOSAR_STACK_MONITORING
FUNC(tpl_bool, OS_CODE) tpl_check_stack_pointer(
  CONST(tpl_proc_id, AUTOMATIC) proc_id)
{
    return 1;
}

tpl_bool tpl_check_stack_footprint(
  CONST(tpl_proc_id, AUTOMATIC) proc_id)
{
    return 1;
}
#endif /* WITH_AUTOSAR_STACK_MONITORING */

/*
 * table which stores the signals used for interrupt
 * handlers.
 */
#ifndef NO_ISR
	extern int signal_for_isr_id[ISR_COUNT];
#endif
#ifdef WITH_AUTOSAR_TIMING_PROTECTION
    const int signal_for_watchdog = SIGALRM;
#endif /* WITH_AUTOSAR_TIMING_PROTECTION */
#if (defined WITH_AUTOSAR && !defined NO_SCHEDTABLE) || (!defined NO_ALARM)
	const int signal_for_counters = SIGUSR2;
#endif

/*
 * The signal set corresponding to the enabled interrupts
 */
sigset_t    signal_set;

/** fonction that calls the system function tpl_counter_tick() 
 * for each counter declared in the application.
 * tpl_call_counter_tick() is application dependant and is 
 * generated by the OIL compiler (goil).
 */
void tpl_call_counter_tick();

/*static sigset_t tpl_saved_state;*/

/**
 * Enable all interrupts
 */
void tpl_enable_interrupts(void)
{
    if ( -1 == sigprocmask(SIG_UNBLOCK, &signal_set, NULL) )
    {
        perror("tpl_enable_interrupt failed");
        exit(-1);
    }
}

/**
 * Disable all interrupts
 */
void tpl_disable_interrupts(void)
{
    if ( -1 == sigprocmask(SIG_BLOCK, &signal_set, NULL) )
    {
        perror("tpl_disable_interrupts failed");
        exit(-1);
    }
}

/*
 * The signal handler used when interrupts are enabled
 */
void tpl_signal_handler(int sig)
{

#ifdef WITH_AUTOSAR_TIMING_PROTECTION
    struct itimerval timer;
#endif /* WITH_AUTOSAR_TIMING_PROTECTION */
#ifndef NO_ISR
            unsigned int id;
            unsigned char found;
#endif

    tpl_locking_depth++;
    tpl_cpt_os_task_lock++;

#if (defined WITH_AUTOSAR && !defined NO_SCHEDTABLE) || (!defined NO_ALARM)
    if (signal_for_counters == sig) 
    {
        tpl_call_counter_tick();
    }
    else
    {
#endif /*(defined WITH_AUTOSAR && !defined NO_SCHEDTABLE) || ... */
#ifdef WITH_AUTOSAR_TIMING_PROTECTION
        if (signal_for_watchdog == sig)
        {
            /* disable the interval timer (one shot) */
            timer.it_value.tv_sec = 0;
            timer.it_value.tv_usec = 0;
            timer.it_interval.tv_sec = 1;
            timer.it_interval.tv_usec = 0;
            setitimer (ITIMER_REAL, &timer, NULL);
      
           tpl_watchdog_expiration();
        }
        else
        {
#endif /* WITH_AUTOSAR_TIMING_PROTECTION */
#ifndef NO_ISR
            id = 0;
            found = (sig == signal_for_isr_id[id]);
            while( (id < ISR_COUNT) && !found)
            {
                id++;
                if(id < ISR_COUNT) 
                {
                    found  = (sig == signal_for_isr_id[id]);
                }
            }/* while((id < ISR_COUNT) && !found) */

            if(found)
            {
                tpl_central_interrupt_handler(id + TASK_COUNT);
            } 
            else
            {
                /* Unknown interrupt request ! */
                printf("No ISR is registered for signal %d\n", sig);
                printf("Cowardly exiting!\n");
                tpl_shutdown();
            }
#endif /* NO_ISR */
#ifdef WITH_AUTOSAR_TIMING_PROTECTION
        }
#endif /* WITH_AUTOSAR_TIMING_PROTECTION */
#if (defined WITH_AUTOSAR && !defined NO_SCHEDTABLE) || (!defined NO_ALARM)
    }
#endif /* (defined WITH_AUTOSAR && !defined NO_SCHEDTABLE) || ... */
	
    tpl_locking_depth--;
    tpl_cpt_os_task_lock--;
	
}

/*
 * tpl_sleep is used by the idle task
 */
void tpl_sleep(void)
{
    while (1) sleep(10); 
}

extern void viper_kill(void);

void tpl_shutdown(void)
{
	/* remove ITs */
	if (sigprocmask(SIG_BLOCK,&signal_set,NULL) == -1)
    {
        perror("tpl_shutdown failed");
        exit(-1);
    }
    
    viper_kill();
    exit(0);
}

volatile int x = 0;
int cnt = 0;
/*
 * tpl_get_task_lock is used to lock a critical section 
 * around the task management in the os.
 */
void tpl_get_task_lock(void)
{
    /*
     * block the handling of signals
     */
    if(0 == tpl_locking_depth) {
        if (sigprocmask(SIG_BLOCK,&signal_set,NULL) == -1)
        {
            perror("tpl_get_lock failed");
            exit(-1);
        }
    }
    tpl_locking_depth++;
    tpl_cpt_os_task_lock++;
}

/*
 * tpl_release_task_lock is used to unlock a critical section
 * around the task management in the os.
 */
void tpl_release_task_lock(void)
{
#if defined(__unix__) || defined(__APPLE__)
	assert( tpl_locking_depth > 0 );
#endif
	tpl_locking_depth--;
    tpl_cpt_os_task_lock--;

    if ( (tpl_locking_depth == 0) && (FALSE == tpl_user_task_lock) )
    {
        if (sigprocmask(SIG_UNBLOCK,&signal_set,NULL) == -1) {
            perror("tpl_release_lock failed");
            exit(-1);
        }
    }
}

#define OS_START_SEC_CODE
#include "tpl_memmap.h"
FUNC(void, OS_CODE) tpl_switch_context(
    CONSTP2CONST(tpl_context, AUTOMATIC, OS_CONST) old_context,
    CONSTP2CONST(tpl_context, AUTOMATIC, OS_CONST) new_context)
{
    if( NULL == old_context)
    {
        _longjmp((*new_context)->current, 1);
    }
    else if ( 0 == _setjmp((*old_context)->current) ) 
    {
        _longjmp((*new_context)->current, 1);
    }
    return;
}


FUNC(void, OS_CODE) tpl_switch_context_from_it(
    CONSTP2CONST(tpl_context, AUTOMATIC, OS_CONST) old_context,
    CONSTP2CONST(tpl_context, AUTOMATIC, OS_CONST) new_context)
{
    if( NULL == old_context )
    {
        _longjmp((*new_context)->current, 1);
    }
    else if ( 0 == _setjmp((*old_context)->current) ) 
    {
        _longjmp((*new_context)->current, 1);
    }
    return;
}

#define OS_START_SEC_CODE
#include "tpl_memmap.h"
FUNC(void, OS_CODE) tpl_init_context(
        CONST(tpl_proc_id, OS_APPL_DATA) proc_id)
{
    /*printf("tpl_init_context(%d)\n", proc_id);*/
    memcpy( tpl_stat_proc_table[proc_id]->context->current,
            tpl_stat_proc_table[proc_id]->context->initial,
            sizeof(jmp_buf));
}

void tpl_osek_func_stub( tpl_proc_id task_id )
{
    tpl_proc_function func = tpl_stat_proc_table[task_id]->entry;
    tpl_proc_type     type = tpl_stat_proc_table[task_id]->type;
#ifdef WITH_AUTOSAR	
	/*  init the error to no error  */
	VAR(StatusType, AUTOMATIC) result = E_OK;
#endif /* WITH_AUTOSAR */
  
    /* Avoid signal blocking due to a previous call to tpl_init_context in a OS_ISR2 context. */
	tpl_release_task_lock();
    
    (*func)();
    
	/* If old process is an ISR2, call TerminateISR2 (in AUTOSAR : enable interrupts and release
	   resources, calling the errorhook (if configured), if needed).
	   If old process is a task :
	   - OSEK : error
	   - AUTOSAR : terminate the task calling errorhook (if configured) and enable interrupts
	   and release resources, if needed
	 */
    if (type == IS_ROUTINE) {
	#ifdef WITH_AUTOSAR	
		/* enable interrupts if disabled */
		if(FALSE!=tpl_get_interrupt_lock_status())  
	    {
			tpl_reset_interrupt_lock_status();
			/*tpl_enable_interrupts(); now ?? or wait until TerminateISR reschedule and interrupts enabled returning previous API service call OR by signal_handler.*/
			result = E_OS_DISABLEDINT;
		}
		/* release resources if held */
		if( (tpl_kern.running->resources) != NULL ){
			tpl_release_all_resources(tpl_kern.running_id);
			result = E_OS_RESOURCE;
		}
		
		PROCESS_ERROR(result);  /* store terminateISR service id before hook ?*/
	#endif /* WITH_AUTOSAR*/
		TerminateISR();
    }
    else {

	#ifdef WITH_AUTOSAR	
		/* enable interrupts if disabled */
		if(FALSE!=tpl_get_interrupt_lock_status())  
		{                                           
			tpl_reset_interrupt_lock_status();
			/*tpl_enable_interrupts(); now ?? or wait until TerminateISR reschedule and interrupts enabled returning previous API service call OR by signal_handler.*/
		}
		/* release resources if held */
		if( (tpl_kern.running->resources) != NULL ){
			tpl_release_all_resources(tpl_kern.running_id);
		}
		
		/* error hook*/
		PROCESS_ERROR(E_OS_MISSINGEND); /* store terminatetask service id before hook ?*/

		/* terminate the task */
		tpl_terminate_task_service();
	#endif /* WITH_AUTOSAR */ 
		
		/*should never come here because the task has to be terminated by the OS*/
        fprintf(stderr, "[OSEK/VDX Spec. 2.2.3 Sec. 4.7] Ending the task without a call to TerminateTask or ChainTask is strictly forbidden and causes undefined behaviour.\n");
        exit(1);
    }
}

/**
 * global variables used to store the "old" context
 * during the trampoline phase used to create a new context
 */
VAR(sigset_t,OS_VAR)        saved_mask;
VAR(sig_atomic_t,OS_VAR)    handler_has_been_triggered;
VAR(tpl_proc_id,OS_VAR)     new_proc_id;

#define OS_START_SEC_CODE
#include "tpl_memmap.h"
FUNC(void, OS_CODE) tpl_create_context_boot(void)
{
    tpl_proc_id context_owner_proc_id;
    
    /*
     * 10 : restore the mask modified by _longjmp so that
     * all tasks are executed with the same mask
     * Commented out since _longjmp/_setjmp replace longjmp/setjmp
     */
     sigprocmask(SIG_SETMASK, &saved_mask, NULL);

    /* 11 : store the id of the owner of this context */
    context_owner_proc_id = new_proc_id;
    /*printf("Just set context_owner_proc_id to %d\n", context_owner_proc_id); */
/*    getchar();*/

    /* 12 & 13 : context is ready, jump back to the tpl_create_context */
    if( 0 == _setjmp(tpl_stat_proc_table[context_owner_proc_id]->context->initial) ) 
    {
        _longjmp(tpl_stat_proc_table[IDLE_TASK_ID]->context->current, 1);
    }

    /*printf("About to launch proc#%d\n", context_owner_proc_id);*/
/*    getchar(); */
    /* 13 bis : save the initial context of the task */
    /*memcpy( tpl_stat_proc_table[context_owner_proc_id]->context->initial,
            tpl_stat_proc_table[context_owner_proc_id]->context->current, 
            sizeof(jmp_buf));
    */
    /* We are back for the first dispatch. Let's go */
/*    tpl_osek_func_stub(context_owner_proc_id); */
    tpl_osek_func_stub(tpl_kern.running_id);

    /* We should not be there. Let's crash*/
    abort();
    return;
}

#define OS_START_SEC_CODE
#include "tpl_memmap.h"
FUNC(void, OS_CODE) tpl_create_context_trampoline(int sigid)
{
    /* 5 : new context created. We go back to tpl_init_context */
    if( 0==_setjmp(tpl_stat_proc_table[new_proc_id]->context->initial) ) 
    {
        handler_has_been_triggered = TRUE;
        return;
    }

    /* 
     * 9 : we are back after a jump, but no more in signal handling mode
     * We are ready to boot the new context with a clean stack 
     */
    tpl_create_context_boot();
    return;
}

#define OS_START_SEC_CODE
#include "tpl_memmap.h"
FUNC(void, OS_CODE) tpl_create_context(
    CONST(tpl_proc_id, OS_APPL_DATA) proc_id)
{
    struct sigaction new_action;
    struct sigaction old_action;
    stack_t new_stack;
    stack_t old_stack;
    sigset_t new_mask;
    sigset_t old_mask;

    /* 1 : save the current mask, and mask our worker signal : SIGUSR1 */
    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &new_mask, &old_mask); 

    /*
     * 2 : install the new action for our worker signal.
     * Triggered function : tpl_init_context_trampoline.
     * It will be executed on its own stack (that is the trick).
     * When it will be executed, all signals will be blocked.
     */
    memset((void*)&new_action, 0, sizeof(new_action));
    new_action.sa_handler = tpl_create_context_trampoline;
    new_action.sa_flags = SA_ONSTACK;
    sigemptyset(&new_action.sa_mask);
    sigaction(SIGUSR1, &new_action, &old_action);

    /* 3 : prepare the new stack */
    new_stack.ss_sp = (tpl_stat_proc_table[proc_id]->stack)->stack_zone;
    new_stack.ss_size = (tpl_stat_proc_table[proc_id]->stack)->stack_size;
    new_stack.ss_flags = 0;
    sigaltstack(&new_stack, &old_stack);

    /* 4-a : store data for new context in globals */
/*    new_env = &((tpl_stat_proc_table[proc_id]->context)->current);*/
    new_proc_id = proc_id;
    saved_mask = old_mask; 
    handler_has_been_triggered = FALSE;
    /* 4-b : send the worker signal */
    kill(getpid(), SIGUSR1);
    /* 4-c : prepare to unblock the worker signal */
    sigfillset(&new_mask);
    sigdelset(&new_mask, SIGUSR1);
    /* 
     * 4-d : unblock the worker signal and wait for it.
     * Once it is arrived, the previous mask is restored.
     */
    while(FALSE == handler_has_been_triggered)
        sigsuspend(&new_mask);

    /*
     * 6 : we are back from tpl_init_context_trampoline.
     * We can restore the previous signal handling configuration
     */
    sigaltstack(NULL, &new_stack);
    new_stack.ss_flags = SS_DISABLE;
    sigaltstack(&new_stack, NULL);
    if( ! (old_stack.ss_flags & SS_DISABLE) ) 
        sigaltstack(&old_stack, NULL);
    sigaction(SIGUSR1, &old_action, NULL);
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    /*
     * 7 & 8 : we jump back to the created context.
     * This time, we are no more in signal handling mode
     */
    if ( 0 == _setjmp(tpl_stat_proc_table[IDLE_TASK_ID]->context->current) )
        _longjmp(tpl_stat_proc_table[new_proc_id]->context->initial,1);

    /* 
     * 14 : we go back to the caller 
     */
    return;
}

void quit(int n)
{
    ShutdownOS(E_OK);  
}

/*
 * tpl_init_machine init the virtual processor hosted in
 * a Unix process
 */
void tpl_init_machine(void)
{
    tpl_proc_id proc_id;
#ifndef NO_ISR
    int id;
#endif
    struct sigaction sa;


    /* create the context of each tpl_proc */
    for(    proc_id = 0; 
            proc_id < TASK_COUNT+ISR_COUNT+1; 
            proc_id++)
    {
        tpl_create_context(proc_id);
    }
    
    signal(SIGINT, quit);
    signal(SIGHUP, quit);

    sigemptyset(&signal_set);
    
    /*
     * init a signal mask to block all signals (aka interrupts)
     */
#ifndef NO_ISR
    for (id = 0; id < ISR_COUNT; id++) {
        sigaddset(&signal_set,signal_for_isr_id[id]);
    }
#endif
#ifdef WITH_AUTOSAR_TIMING_PROTECTION
    sigaddset(&signal_set,signal_for_watchdog);
#endif /* WITH_AUTOSAR_TIMING_PROTECTION */
#if (defined WITH_AUTOSAR && !defined NO_SCHEDTABLE) || (!defined NO_ALARM)
    sigaddset(&signal_set,signal_for_counters);
#endif /*(defined WITH_AUTOSAR && !defined NO_SCHEDTABLE) || ... */


    /*
     * init the sa structure to install the handler
     */
    sa.sa_handler = tpl_signal_handler;
    sa.sa_mask = signal_set;
    sa.sa_flags = SA_RESTART;
    /*
     * Install the signal handler used to emulate interruptions
     */
#ifndef NO_ISR
    for (id = 0; id < ISR_COUNT; id++) {
        sigaction(signal_for_isr_id[id],&sa,NULL);
    }
#endif
#ifdef WITH_AUTOSAR_TIMING_PROTECTION
    sigaction(signal_for_watchdog,&sa,NULL);
#endif /* WITH_AUTOSAR_TIMING_PROTECTION */
#if (defined WITH_AUTOSAR && !defined NO_SCHEDTABLE) || (!defined NO_ALARM)
    sigaction(signal_for_counters,&sa,NULL);
#endif /*(defined WITH_AUTOSAR && !defined NO_SCHEDTABLE) || ... */

    tpl_viper_init();
    usleep(1000);
#if (defined WITH_AUTOSAR && !defined NO_SCHEDTABLE) || (!defined NO_ALARM)
    tpl_viper_start_auto_timer(signal_for_counters,10000);  /* 10 ms */
#endif

    /*
     * unblock the handling of signals
     */
    /*if (sigprocmask(SIG_UNBLOCK,&signal_set,NULL) == -1) {
        perror("tpl_init_machine failed");
        exit(-1);
    }
	*/
#ifdef WITH_AUTOSAR_TIMING_PROTECTION
    gettimeofday (&startup_time, NULL);  
#endif /* WITH_AUTOSAR_TIMING_PROTECTION */
}


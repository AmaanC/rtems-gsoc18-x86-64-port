/*
 *  Stack Overflow Check User Extension Set
 *
 *  NOTE:  This extension set automatically determines at
 *         initialization time whether the stack for this
 *         CPU grows up or down and installs the correct
 *         extension routines for that direction.
 *
 *  COPYRIGHT (c) 1989, 1990, 1991, 1992, 1993, 1994.
 *  On-Line Applications Research Corporation (OAR).
 *  All rights assigned to U.S. Government, 1994.
 *
 *  This material may be reproduced by or for the U.S. Government pursuant
 *  to the copyright license under the clause at DFARS 252.227-7013.  This
 *  notice must appear in all copies of this file and its derivatives.
 *
 *  $Id$
 *
 */

#include <rtems/system.h>
#include <rtems/extension.h>
#include <rtems/fatal.h>
#include <rtems/heap.h>
#include <rtems/stack.h>
#include <rtems/thread.h>
#ifdef XXX_RTEMS_H_FIXED
#include <bsp.h>
#else
#include <rtems/config.h>
extern rtems_configuration_table BSP_Configuration;
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "stackchk.h"
#include "internal.h"

/*
 *  This variable contains the name of the task which "blew" the stack.
 *  It is NULL if the system is all right.
 */

Thread_Control *Stack_check_Blown_task;

/*
 *  The extension table for the stack checker.
 */

rtems_extensions_table Stack_check_Extension_table = {
  Stack_check_Create_extension,     /* rtems_task_create  */
  0,                                /* rtems_task_start   */
  0,                                /* rtems_task_restart */
  0,                                /* rtems_task_delete  */
  Stack_check_Switch_extension,     /* task_switch  */
  Stack_check_Begin_extension,      /* task_begin   */
  0,                                /* task_exitted */
  Stack_check_Fatal_extension,      /* fatal        */
};

/*
 *  The "magic pattern" used to mark the end of the stack.
 */

Stack_check_Control Stack_check_Pattern;

/*
 *  Where the pattern goes in the stack area is dependent upon
 *  whether the stack grow to the high or low area of the memory.
 *
 */

#if ( CPU_STACK_GROWS_UP == TRUE )

#define Stack_check_Get_pattern_area( _the_stack ) \
  ((Stack_check_Control *) \
    ((_the_stack)->area + (_the_stack)->size - sizeof( Stack_check_Control ) ))

#define Stack_check_Calculate_used( _low, _size, _high_water ) \
    ((_high_water) - (_low))

#define Stack_check_usable_stack_start(_the_stack) \
    ((_the_stack)->area)

#else

#define Stack_check_Get_pattern_area( _the_stack ) \
  ((Stack_check_Control *) ((_the_stack)->area + HEAP_OVERHEAD))

#define Stack_check_Calculate_used( _low, _size, _high_water) \
    ( ((_low) + (_size)) - (_high_water) )

#define Stack_check_usable_stack_start(_the_stack) \
    ((_the_stack)->area + sizeof(Stack_check_Control))

#endif

#define Stack_check_usable_stack_size(_the_stack) \
    ((_the_stack)->size - sizeof(Stack_check_Control))


/*
 * Do we have an interrupt stack?
 * XXX it would sure be nice if the interrupt stack were also
 *     stored in a "stack" structure!
 */


Stack_Control stack_check_interrupt_stack;

/*
 * Fill an entire stack area with BYTE_PATTERN.
 * This will be used by a Fatal extension to check for
 * amount of actual stack used
 */

void
stack_check_dope_stack(Stack_Control *stack)
{
    memset(stack->area, BYTE_PATTERN, stack->size);
}


/*PAGE
 *
 *  Stack_check_Initialize
 */

unsigned32 stack_check_initialized = 0;

void Stack_check_Initialize( void )
{
  rtems_status_code status;
  Objects_Id   id_ignored;
  unsigned32  *p;

  if (stack_check_initialized)
      return;

  /*
   * Dope the pattern and fill areas
   */

  for ( p = Stack_check_Pattern.pattern;
        p < &Stack_check_Pattern.pattern[PATTERN_SIZE_WORDS];
        p += 4
      )
  {
      p[0] = 0xFEEDF00D;          /* FEED FOOD to BAD DOG */
      p[1] = 0x0BAD0D06;
      p[2] = 0xDEADF00D;          /* DEAD FOOD GOOD DOG */
      p[3] = 0x600D0D06;
  };

  status = rtems_extension_create(
    rtems_build_name( 'S', 'T', 'C', 'K' ),
    &Stack_check_Extension_table,
    &id_ignored
  );
  assert ( status == RTEMS_SUCCESSFUL );

  Stack_check_Blown_task = 0;

  /*
   * If installed by a task, that task will not get setup properly
   * since it missed out on the create hook.  This will cause a
   * failure on first switch out of that task.
   * So pretend here that we actually ran create and begin extensions.
   */

  if (_Thread_Executing)
  {
      Stack_check_Create_extension(_Thread_Executing, _Thread_Executing);
  }

  /*
   * If appropriate, setup the interrupt stack for high water testing
   * also.
   */
  if (_CPU_Interrupt_stack_low && _CPU_Interrupt_stack_high)
  {
      stack_check_interrupt_stack.area = _CPU_Interrupt_stack_low;
      stack_check_interrupt_stack.size = _CPU_Interrupt_stack_high -
                                               _CPU_Interrupt_stack_low;

      stack_check_dope_stack(&stack_check_interrupt_stack);
  }

  stack_check_initialized = 1;
}

/*PAGE
 *
 *  Stack_check_Create_extension
 */

void Stack_check_Create_extension(
  Thread_Control *running,
  Thread_Control *the_thread
)
{
    if (the_thread && (the_thread != _Thread_Executing))
        stack_check_dope_stack(&the_thread->Start.Initial_stack);
}

/*PAGE
 *
 *  Stack_check_Begin_extension
 */

void Stack_check_Begin_extension(
  Thread_Control *the_thread
)
{
  Stack_check_Control  *the_pattern;

  if ( the_thread->Object.id == 0 )        /* skip system tasks */
    return;

  the_pattern = Stack_check_Get_pattern_area(&the_thread->Start.Initial_stack);

  *the_pattern = Stack_check_Pattern;
}

/*PAGE
 *
 *  Stack_check_report_blown_task
 *  Report a blown stack.  Needs to be a separate routine
 *  so that interrupt handlers can use this too.
 *
 *  Caller must have set the Stack_check_Blown_task.
 *
 *  NOTE: The system is in a questionable state... we may not get
 *        the following message out.
 */

void Stack_check_report_blown_task(void)
{
    Stack_Control *stack;
    Thread_Control *running;

    running = Stack_check_Blown_task;
    stack = &running->Start.Initial_stack;

    fprintf(
        stderr,
        "BLOWN STACK!!! Offending task(%p): id=0x%08x; name=0x%08x",
        running,
        running->Object.id,
        running->name);
    fflush(stderr);

    if (BSP_Configuration.User_multiprocessing_table)
        fprintf(
          stderr,
          "; node=%d\n",
          BSP_Configuration.User_multiprocessing_table->node
       );
    else
        fprintf(stderr, "\n");
    fflush(stderr);

    fprintf(
        stderr,
        "  stack covers range 0x%08x - 0x%08x (%d bytes)\n",
        (unsigned32) stack->area,
        (unsigned32) stack->area + stack->size - 1,
        (unsigned32) stack->size);
    fflush(stderr);

    fprintf(
        stderr,
        "  Damaged pattern begins at 0x%08x and is %d bytes long\n",
        (unsigned32) Stack_check_Get_pattern_area(stack), PATTERN_SIZE_BYTES);
    fflush(stderr);

    rtems_fatal_error_occurred( (unsigned32) "STACK BLOWN" );
}

/*PAGE
 *
 *  Stack_check_Switch_extension
 */

void Stack_check_Switch_extension(
  Thread_Control *running,
  Thread_Control *heir
)
{
  if ( running->Object.id == 0 )        /* skip system tasks */
    return;

  if (0 != memcmp( (void *) Stack_check_Get_pattern_area( &running->Start.Initial_stack)->pattern,
                  (void *) Stack_check_Pattern.pattern,
                  PATTERN_SIZE_BYTES))
  {
      Stack_check_Blown_task = running;
      Stack_check_report_blown_task();
  }
}

void *Stack_check_find_high_water_mark(
  const void *s,
  size_t n
)
{
  const unsigned32 *base, *ebase;
  unsigned32 length;

  base = s;
  length = n/4;

#if ( CPU_STACK_GROWS_UP == TRUE )
  /*
   * start at higher memory and find first word that does not
   * match pattern
   */

  base += length - 1;
  for (ebase = s; base > ebase; base--)
      if (*base != U32_PATTERN)
          return (void *) base;
#else
  /*
   * start at lower memory and find first word that does not
   * match pattern
   */

  base += 4;
  for (ebase = base + length; base < ebase; base++)
      if (*base != U32_PATTERN)
          return (void *) base;
#endif

  return (void *)0;
}

/*PAGE
 *
 *  Stack_check_Dump_threads_usage
 *  Try to print out how much stack was actually used by the task.
 *
 */

void Stack_check_Dump_threads_usage(
  Thread_Control *the_thread
)
{
  unsigned32      size, used;
  void           *low;
  void           *high_water_mark;
  Stack_Control  *stack;

  if ( !the_thread )
    return;

  /*
   * XXX HACK to get to interrupt stack
   */

  if (the_thread == (Thread_Control *) -1)
  {
      if (stack_check_interrupt_stack.area)
      {
          stack = &stack_check_interrupt_stack;
          the_thread = 0;
      }
      else
          return;
  }
  else
      stack = &the_thread->Start.Initial_stack;

  low  = Stack_check_usable_stack_start(stack);
  size = Stack_check_usable_stack_size(stack);

  high_water_mark = Stack_check_find_high_water_mark(low, size);

  if ( high_water_mark )
    used = Stack_check_Calculate_used( low, size, high_water_mark );
  else
    used = 0;

  printf( "0x%08x  0x%08x  0x%08x  0x%08x   %8d   %8d\n",
          the_thread ? the_thread->Object.id : ~0,
          the_thread ? the_thread->name :
                       rtems_build_name('I', 'N', 'T', 'R'),
          (unsigned32) stack->area,
          (unsigned32) stack->area + (unsigned32) stack->size - 1,
          size,
          used
  );
}

/*PAGE
 *
 *  Stack_check_Fatal_extension
 */

void Stack_check_Fatal_extension( unsigned32 status )
{
    if (status == 0)
        Stack_check_Dump_usage();
}


/*PAGE
 *
 *  Stack_check_Dump_usage
 */

void Stack_check_Dump_usage( void )
{
  unsigned32      i;
  Thread_Control *the_thread;
  unsigned32      hit_running = 0;

  if (stack_check_initialized == 0)
      return;

  printf(
    "   ID          NAME         LOW        HIGH      AVAILABLE     USED\n"
  );
  for ( i=1 ; i<_Thread_Information.maximum ; i++ ) {
    the_thread = (Thread_Control *)_Thread_Information.local_table[ i ];
    Stack_check_Dump_threads_usage( the_thread );
    if ( the_thread == _Thread_Executing )
      hit_running = 1;
  }

  if ( !hit_running )
    Stack_check_Dump_threads_usage( _Thread_Executing );

  /* dump interrupt stack info if any */
  Stack_check_Dump_threads_usage((Thread_Control *) -1);
}


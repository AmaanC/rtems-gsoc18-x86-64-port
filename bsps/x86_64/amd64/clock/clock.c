/*
 * Copyright (c) 2018.
 * Amaan Cheval <amaan.cheval@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <bsp.h>
#include <rtems.h>
#include <rtems/score/interrupts.h>
#include <rtems/timecounter.h>
#include <rtems/score/cpu.h>
#include <rtems/score/cpuimpl.h>
#include <rtems/score/x86_64.h>
#include <bsp/irq-generic.h>

/* Use the amd64_apic_base as a pointer into an array of 32-bit APIC registers */
uint32_t *amd64_apic_base;
static struct timecounter amd64_clock_tc;

extern volatile uint32_t Clock_driver_ticks;
extern void apic_spurious_handler(void);
extern void Clock_isr(void *param);

static uint32_t amd64_clock_get_timecount(struct timecounter *tc)
{
  return Clock_driver_ticks;
}

/*
 * When the CPUID instruction is executed with a source operand of 1 in the EAX
 * register, bit 9 of the CPUID feature flags returned in the EDX register
 * indicates the presence (set) or absence (clear) of a local APIC.
 */
bool has_apic_support()
{
  uint32_t eax, ebx, ecx, edx;
  cpuid(1, &eax, &ebx, &ecx, &edx);
  return (edx >> 9) & 1;
}

/*
 * Initializes the APIC by hardware and software enabling it, and sets up the
 * amd64_apic_base pointer that can be used as a 32-bit addressable array to
 * access APIC registers.
 */
void apic_initialize(void)
{
  if (!has_apic_support()) {
    printf("cpuid claims no APIC support; trying anyway.\n");
  }

  /*
   * The APIC base address is a 36-bit physical address.
   * We have identity-paging setup at the moment, which makes this simpler, but
   * that's something to note since the variables below use virtual addresses.
   *
   * Bits 0-11 (inclusive) are 0, making the address page (4KiB) aligned.
   * Bits 12-35 (inclusive) of the MSR point to the rest of the address.
   */
  uint64_t apic_base_msr = rdmsr(APIC_BASE_MSR);
  amd64_apic_base = (uint32_t*) apic_base_msr;
  amd64_apic_base = (uintptr_t) amd64_apic_base & 0x0ffffff000;

  /* Hardware enable the APIC just to be sure */
  wrmsr(
    APIC_BASE_MSR,
    apic_base_msr | APIC_BASE_MSR_ENABLE,
    apic_base_msr >> 32
  );

#if 1
  printf("APIC is at %x\n", (uintptr_t) amd64_apic_base);
  printf("APIC ID at *%x=%x\n", &amd64_apic_base[APIC_REGISTER_APICID], amd64_apic_base[APIC_REGISTER_APICID]);

  printf("APIC spurious vector register *%x=%x\n", &amd64_apic_base[APIC_REGISTER_SPURIOUS], amd64_apic_base[APIC_REGISTER_SPURIOUS]);
#endif

  /*
   * Software enable the APIC by mapping spurious vector and setting enable bit.
   */
  uintptr_t old;
  amd64_install_raw_interrupt(BSP_VECTOR_SPURIOUS, apic_spurious_handler, &old);
  amd64_apic_base[APIC_REGISTER_SPURIOUS] = APIC_SPURIOUS_ENABLE | BSP_VECTOR_SPURIOUS;

#if 1
  printf("APIC spurious vector register *%x=%x\n", &amd64_apic_base[APIC_REGISTER_SPURIOUS], amd64_apic_base[APIC_REGISTER_SPURIOUS]);
#endif

  /*
   * The PIC may send spurious IRQs even when disabled, and without remapping
   * IRQ7 would look like an exception.
   */
  pic_remap(PIC1_REMAP_DEST, PIC2_REMAP_DEST);
  pic_disable();
}

void apic_isr(void *param)
{
  Clock_isr(param);
  amd64_apic_base[APIC_REGISTER_EOI] = APIC_EOI_ACK;
}

void apic_timer_install_handler(void)
{
  rtems_status_code sc = rtems_interrupt_handler_install(
    BSP_VECTOR_APIC_TIMER,
    "APIC timer",
    RTEMS_INTERRUPT_UNIQUE,
    apic_isr,
    NULL
  );
  assert(sc == RTEMS_SUCCESSFUL);
}

uint64_t apic_timer_initialize(uint64_t irq_ticks_per_sec)
{
  /* Configure APIC timer in one-shot mode to prepare for calibration */
  amd64_apic_base[APIC_REGISTER_LVT_TIMER] = BSP_VECTOR_APIC_TIMER;
  amd64_apic_base[APIC_REGISTER_TIMER_DIV] = APIC_TIMER_SELECT_DIVIDER;

  /* Enable the channel 2 timer gate and disable the speaker output */
  uint8_t chan2_value = (inport_byte(PIT_PORT_CHAN2_GATE) | PIT_CHAN2_TIMER_BIT) & ~PIT_CHAN2_SPEAKER_BIT;
  outport_byte(PIT_PORT_CHAN2_GATE, chan2_value);

  /* Initialize PIT in one-shot mode on Channel 2 */
  outport_byte(
    PIT_PORT_MCR,
    PIT_SELECT_CHAN2 | PIT_SELECT_ACCESS_LOHI |
      PIT_SELECT_ONE_SHOT_MODE | PIT_SELECT_BINARY_MODE
  );

  /*
   * Disable interrupts while we calibrate for 2 reasons:
   *   - Writing values to the PIT should be atomic (for now, this is okay
   *     because we're the only ones ever touching the PIT ports)
   *   - We need to make sure that no interrupts throw our synchronization of
   *     the APIC timer off.
   */
  amd64_disable_interrupts();

  /* Since the PIT only has 2 one-byte registers, the maximum tick value is
   * limited. We can set the PIT to use a frequency divider if needed. */
  assert(PIT_CALIBRATE_TICKS <= 0xffff);

  /* Set PIT reload value */
  uint32_t pit_ticks = PIT_CALIBRATE_TICKS;
  uint8_t low_tick = pit_ticks & 0xff;
  uint8_t high_tick = (pit_ticks >> 8) & 0xff;

  outport_byte(PIT_PORT_CHAN2, low_tick);
  stub_io_wait();
  outport_byte(PIT_PORT_CHAN2, high_tick);

  /* Restart PIT by disabling the gated input and then re-enabling it */
  chan2_value &= ~PIT_CHAN2_TIMER_BIT;
  outport_byte(PIT_PORT_CHAN2_GATE, chan2_value);
  chan2_value |= PIT_CHAN2_TIMER_BIT;
  outport_byte(PIT_PORT_CHAN2_GATE, chan2_value);

  /* Start APIC timer's countdown */
  const uint32_t apic_calibrate_init_count = 0xffffffff;
  amd64_apic_base[APIC_REGISTER_TIMER_INITCNT] = apic_calibrate_init_count;

  while(pit_ticks <= PIT_CALIBRATE_TICKS)
  {
    /* Send latch command to read multi-byte value atomically */
    outport_byte(PIT_PORT_MCR, PIT_SELECT_CHAN2);
    pit_ticks = inport_byte(PIT_PORT_CHAN2);
    pit_ticks |= inport_byte(PIT_PORT_CHAN2) << 8;
  }
  uint32_t apic_currcnt = amd64_apic_base[APIC_REGISTER_TIMER_CURRCNT];
  /* Stop APIC timer to calculate ticks to time ratio */
  amd64_apic_base[APIC_REGISTER_LVT_TIMER] = APIC_DISABLE;

  /* Re-enable interrupts now that calibration is complete */
  amd64_enable_interrupts();

  /* Get counts passed since we started counting */
  uint32_t amd64_apic_ticks_per_sec = apic_calibrate_init_count - apic_currcnt;
  printf("APIC ticks passed in 1/%d of a second: %x\n", PIT_CALIBRATE_DIVIDER, amd64_apic_ticks_per_sec);
  /* We ran the PIT for a fraction of a second */
  amd64_apic_ticks_per_sec = amd64_apic_ticks_per_sec * PIT_CALIBRATE_DIVIDER;

  assert(amd64_apic_ticks_per_sec != 0 && amd64_apic_ticks_per_sec != apic_calibrate_init_count);

  /* Multiply to undo effects of divider */
  uint64_t cpu_bus_frequency = amd64_apic_ticks_per_sec * APIC_TIMER_DIVIDE_VALUE;

  printf("CPU frequency: 0x%llx\nAPIC ticks/sec: 0x%llx", cpu_bus_frequency, amd64_apic_ticks_per_sec);

  /*
   * The APIC timer counter is decremented at the speed of the CPU bus
   * frequency.
   *
   * This means:
   *   cpu_time_per_tick = 1 / (cpu_bus_frequency / timer_divide_value)
   *
   * Therefore:
   *   reload_value * cpu_timer_per_tick = (1/apic_timer_frequency)
   */
  uint32_t apic_timer_reload_value = (cpu_bus_frequency / APIC_TIMER_DIVIDE_VALUE) / irq_ticks_per_sec;

  amd64_apic_base[APIC_REGISTER_LVT_TIMER] = BSP_VECTOR_APIC_TIMER | APIC_SELECT_TMR_PERIODIC;
  amd64_apic_base[APIC_REGISTER_TIMER_DIV] = APIC_TIMER_SELECT_DIVIDER;
  amd64_apic_base[APIC_REGISTER_TIMER_INITCNT] = apic_timer_reload_value;

  return amd64_apic_ticks_per_sec;
}

void amd64_clock_initialize(void)
{
  uint64_t us_per_tick = rtems_configuration_get_microseconds_per_tick();
  uint64_t irq_ticks_per_sec = 1000000 / us_per_tick;
  printf("us_per_tick = %d\nDesired frequency = %d irqs/sec\n", us_per_tick, irq_ticks_per_sec);

  /* Setup and enable the APIC itself */
  apic_initialize();
  /* Setup and initialize the APIC timer */
  uint64_t apic_freq = apic_timer_initialize(irq_ticks_per_sec);

  amd64_clock_tc.tc_get_timecount = amd64_clock_get_timecount;
  amd64_clock_tc.tc_counter_mask = 0xffffffff;
  amd64_clock_tc.tc_frequency = irq_ticks_per_sec;
  amd64_clock_tc.tc_quality = RTEMS_TIMECOUNTER_QUALITY_CLOCK_DRIVER;
  rtems_timecounter_install(&amd64_clock_tc);
}

#define Clock_driver_support_install_isr(_new) \
  apic_timer_install_handler()

#define Clock_driver_support_initialize_hardware() \
  amd64_clock_initialize()

#include "../../../shared/dev/clock/clockimpl.h"

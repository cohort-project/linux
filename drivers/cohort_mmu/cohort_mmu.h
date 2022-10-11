#ifndef COHORT_MMU_H
#define COHORT_MMU_H

#include <linux/mmu_notifier.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/mm_types.h>
#include <linux/profile.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/mmu_notifier.h>
#include <linux/iommu.h>
#include <linux/wait.h>
#include <linux/pci.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/ioport.h> 
#include <asm-generic/io.h> 
#include <linux/of.h>

#include "dcpn_compressed.h"

#ifndef SERIALIZATION_VAL
#define SERIALIZATION_VAL 1
#endif

#ifndef DESERIALIZATION_VAL
#define DESERIALIZATION_VAL 1
#endif

#ifndef WAIT_COUNTER_VAL 
#define WAIT_COUNTER_VAL 1
#endif

#ifndef BACKOFF_COUNTER_VAL
#define BACKOFF_COUNTER_VAL 0x10
#endif

// from dcp_alloc.h
uint64_t alloc_tile(uint64_t tiles, uint64_t * base);
uint64_t dealloc_tiles(void);
 
// from dcpn_compressed.h
uint64_t dec_get_tlb_fault(uint64_t tile);
void dec_resolve_page_fault(uint64_t tile, uint64_t conf_tlb_entry);
int invalidate_tlb_start (struct mmu_notifier *mn, const struct mmu_notifier_range *range);
void invalidate_tlb_end (struct mmu_notifier *mn, const struct mmu_notifier_range *range);

// Cohort's Kernel API
void fifo_start(uint64_t head_ptr, uint64_t meta_ptr, uint64_t tail_ptr, bool cohort_to_sw);
void baremetal_write(uint32_t tile, uint64_t addr, uint64_t value);
uint64_t uncached_read(uint32_t tile, uint64_t addr);

void cohort_on(uint64_t c_head, uint64_t p_head, uint64_t acc_addr, uint64_t backoff_val);
void cohort_off(void);
void cohort_stop_monitors(void);
void cohort_print_monitors(void);
void cohort_print_debug_monitors(void);

void fifo_start(uint64_t head_ptr, uint64_t meta_ptr, uint64_t tail_ptr, bool cohort_to_sw)
{
    PRINTBT

    if (cohort_to_sw) {
        // baremetal_write( 0, 3, head_ptr);
        baremetal_write( 0, 3, head_ptr);
        baremetal_write( 0, 4, meta_ptr);
        baremetal_write( 0, 5, tail_ptr);
    } else { 
        baremetal_write( 0, 0, tail_ptr); // this is actually tail for consumer
        baremetal_write( 0, 1, meta_ptr);
        baremetal_write( 0, 2, head_ptr); 
    }

    __sync_synchronize();

}

void baremetal_write(uint32_t tile, uint64_t addr, uint64_t value){
	PRINTBT
    uint64_t write_addr = (addr*8) | (uint64_t)dream_base[tile]; 

    #ifdef PRINT_ADDR
	printk("Target DREAM addr: %lx, write config data: %lx\n", write_addr, (uint64_t)value);
    #endif

    iowrite64(value, write_addr);
}

uint64_t uncached_read(uint32_t tile, uint64_t addr){
	PRINTBT
    uint64_t read_addr = (addr*8) | (uint64_t) dream_base[tile]; 
	uint64_t read_val = ioread64((void*) read_addr);
	
    #ifdef PRI
	printk("Read value: %llx, from DREAM addr: %llx\n", read_val, read_addr);
    #endif

    return read_val;
}

void cohort_on(uint64_t c_head, uint64_t p_head, uint64_t acc_addr, uint64_t backoff_val)
{   
    PRINTBT
    // Send queue addresses
    uint64_t offset   = 128;
    
	fifo_start(c_head, c_head + offset, c_head + 2*offset, 0);
	fifo_start(p_head, p_head + offset, p_head + 2*offset, 1);    

    // Send acc-r address
    baremetal_write(0, 6, acc_addr);

    // Turn on the monitor
    // Don't lower reset, but turn on and clear the monitor
    uint64_t write_value = 6;
    // uint64_t serialization_value = SERIALIZATION_VAL;
    // uint64_t deserialization_value = DESERIALIZATION_VAL;
    // uint64_t wait_counter = WAIT_COUNTER_VAL;
    // uint64_t backoff_counter = BACKOFF_COUNTER_VAL;

    // write_value |= backoff_counter << 48;
    // write_value |= serialization_value << 32;
    // write_value |= deserialization_value << 16;
    // write_value |= wait_counter << 4;
    // printk("d\n");
    baremetal_write(0, 7, write_value);
    __sync_synchronize();
	
    // Clear counter and turn on the monitor - later might move to user mode
	unsigned long long int write_value_mon = 11;
    uint64_t serialization_value_mon = SERIALIZATION_VAL;
    uint64_t deserialization_value_mon = DESERIALIZATION_VAL;
    uint64_t wait_counter_mon = WAIT_COUNTER_VAL;
    uint64_t backoff_counter_mon = backoff_val;

    write_value_mon |= backoff_counter_mon << 48;
    write_value_mon |= serialization_value_mon << 32;
    write_value_mon |= deserialization_value_mon << 16;
    write_value_mon |= wait_counter_mon << 4;

    // printk("d\n");
    baremetal_write(0, 7, write_value_mon);
    // printk("d\n");
}

void cohort_off(void)
{
    PRINTBT
    cohort_stop_monitors();

    #ifdef COHORT_DEBUG
	cohort_print_monitors(c_id);
	cohort_print_debug_monitors(c_id);
    #endif

    // Turn off the monitor
    baremetal_write(0, 7, 0);
    __sync_synchronize();

    dec_def_flush_tlb(0);

}

void cohort_stop_monitors(void)
{
    PRINTBT
    // Stop counter, but keep reset low
    baremetal_write(0, 7, 1);
}

void cohort_print_monitors(void)
{
    PRINTBT
	int i;
    for (i=0;i< 35; i++) {
        printk("%lx,",uncached_read(0, i));
    }

}

void cohort_print_debug_monitors(void)
{
    PRINTBT
    printk("here's the debug registers dump\n");
    int i;
	for (i=35;i< 55; i++) {
        printk("dbg reg %d: %lx\n",i - 35, uncached_read(0, i));
    }

}

#endif // COHORT_MMU_H
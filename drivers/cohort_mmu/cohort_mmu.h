#ifndef _COHORT_MMU_H
#define _COHORT_MMU_H

#include <linux/mmu_notifier.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/delay.h>

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
#include <linux/ioport.h> // does not link somehow
#include <asm-generic/io.h> 
#include "dcpn_compressed.h"
#include "cohort_types.h"

#define BYTE         8

#define MAX_TILES    2
#define BASE_MMU     0xe100A00000L
#define TILE_X       1
#define TILE_Y       1
#define WIDTH        1 // --> clarify this
#define FIFO         2 // --> clarify this
#define COHORT_TILE  1  // x=1, y=0

#ifndef PRODUCER_FIFO_LENGTH 
#define PRODUCER_FIFO_LENGTH 32
#endif

#ifndef CONSUMER_FIFO_LENGTH 
#define CONSUMER_FIFO_LENGTH 32
#endif

#ifndef WAIT_COUNTER_VAL 
#define WAIT_COUNTER_VAL 128
#endif

#ifndef SERIALIZATION_VAL
#define SERIALIZATION_VAL 1
#endif

#ifndef DESERIALIZATION_VAL
#define DESERIALIZATION_VAL 1
#endif

#ifndef BACKOFF_COUNTER_VAL
#define BACKOFF_COUNTER_VAL 8
#endif

// from dcp_alloc.h
uint64_t alloc_tile(uint64_t tiles, uint64_t * base);
uint64_t dealloc_tile(uint64_t * base, uint64_t * mmub);

// from dcpn_compressed.h
uint64_t dec_get_tlb_fault(uint64_t tile);
// void dec_flush_tlb (struct mmu_notifier *mn,
// 				                            struct mm_struct *mm,
// 				                            unsigned long start, unsigned long end);

void dec_resolve_page_fault(uint64_t tile, uint64_t conf_tlb_entry);

// code for Benchmarks

void cohort_print_monitors(void);

/**
 * an uncached write that applies to bare metal and linux alike
 */
void baremetal_write(uint32_t tile, uint64_t addr, uint64_t value);

uint64_t uncached_read(uint32_t tile, uint64_t addr);
/**
 * turn on cohort
 */
void cohort_on(uint64_t c_head, uint64_t c_meta, uint64_t c_tail, 
						uint64_t p_tail, uint64_t p_meta, uint64_t p_head);

/**
 * turn off cohort
 */
void cohort_off(void);

// void fifo_start(fifo_ctrl_t *fifo_ctrl, bool is_consumer);

//TODO: 128 bits are not supported, see https://github.com/rust-lang/rust/issues/54341

void fifo_start(uint64_t head_ptr, uint64_t meta_ptr, uint64_t tail_ptr, bool cohort_to_sw)
{
    PRINTBT

    if (cohort_to_sw) {
        baremetal_write( 0, 3, head_ptr);
        baremetal_write( 0, 4, meta_ptr);
        baremetal_write( 0, 5, tail_ptr);
    } else { 
        baremetal_write( 0, 0, head_ptr); // this is actually tail for consumer
        baremetal_write( 0, 1, meta_ptr);
        baremetal_write( 0, 2, tail_ptr); 
    }
    __sync_synchronize();
}

void baremetal_write(uint32_t tile, uint64_t addr, uint64_t value){
	PRINTBT
	uint32_t tileno = (tile)*2+1;
	uint64_t base_dream = 0xe100B00000ULL | ((tileno) << 28) | ((0) << 24); 
    void * vaddr = ioremap(base_dream, PG_SIZE);

	// uint64_t write_addr = (addr*8) | base[tileno]; 
	uint64_t write_addr = (addr*8) | (uint64_t)vaddr; 

    #ifdef PRI
	printk("Target DREAM addr: %lx, write config data: %lx\n", write_addr, (uint64_t)value);
    #endif

	// iowrite64(value, (void*) write_addr);
    // writel(value, write_addr);
    iowrite64(value, write_addr);
    iounmap(vaddr);
}

uint64_t uncached_read(uint32_t tile, uint64_t addr){
	PRINTBT
	uint32_t tileno = tile*2+1;
	uint64_t base_dream = 0xe100B00000ULL | ((tileno) << 28) | ((0) << 24); 
    void * vaddr = ioremap(base_dream, PG_SIZE);

	// uint64_t read_addr = (addr*8) | base[tile]; 
	uint64_t read_addr = (addr*8) | (uint64_t) vaddr; 
	uint64_t read_val;

	// read_val = ioread64((void*) read_addr);
	// read_val = readl((void*) read_addr);
	read_val = ioread64((void*) read_addr);
	
    #ifdef PRI
	printk("after ioread64 value = %llx, leaving uncached_read\n", read_val);
    #endif

    return read_val;
}

void cohort_on(uint64_t c_head, uint64_t c_meta, uint64_t c_tail, 
			   uint64_t p_head, uint64_t p_meta, uint64_t p_tail)
{
    PRINTBT

	printk("Before input fifo start\n");
	fifo_start(c_tail, c_meta, c_head, 0);
	printk("Before output fifo start\n");
	fifo_start(p_head, p_meta, p_tail, 1);

    // ---> clarify this acc interface
    // void *acc_address = memalign(128, 128);
    // memset(acc_address, 0, 128);

	// printk("Before acc baremetal\n");
    // baremetal_write(0, 6, (uint64_t) acc_address);

    // turn on the monitor
    // don't lower reset, but turn on and clear the monitor
    baremetal_write(0, 7, 6);
    __sync_synchronize();

	// sleep(4);
    // msleep(2000);
    printk("after cohort on\n");

    // clear counter and turn on the monitor - later might move to user mode
	unsigned long long int write_value = 11;
    unsigned long long int serialization_value = SERIALIZATION_VAL;
    unsigned long long int deserialization_value = DESERIALIZATION_VAL;
    unsigned long long int wait_counter = WAIT_COUNTER_VAL;
    unsigned long long int backoff_counter = 0x800;

    write_value |= backoff_counter << 48;
    write_value |= serialization_value << 32;
    write_value |= deserialization_value << 16;
    write_value |= wait_counter << 4;
    __sync_synchronize();

    baremetal_write(0, 7, write_value);

	// sleep(4);
    // msleep(2000);
    printk("after cohort start 2\n");
}

void cohort_off(void)
{
    PRINTBT
    // turn off the monitor
    baremetal_write(0, 7, 0);
    __sync_synchronize();

    dec_def_flush_tlb(0);

}

void cohort_stop_monitors(void)
{
    PRINTBT
    // stop counter, but keep reset low
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

#endif
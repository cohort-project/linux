
#ifndef __DCP_ALLOC_H
#define __DCP_ALLOC_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <asm/io.h>

#define MEM_REGION 0x00bf000000
#define PG_SIZE ((unsigned long)(1 << 12))

uint64_t alloc_tile(uint64_t tiles, uint64_t * base_addr) {
    PRINTBT
    uint64_t i;
    struct resource *res;

    __sync_synchronize();

    for (i = 0; i < tiles; i++){
        res = request_mem_region((unsigned long)base_addr[i], PG_SIZE, DRIVER_NAME);

        if (res == NULL){
            return i;
        }

        #ifdef PRI
        printk("Physical address is %llx\n", (unsigned long)base_addr[i]);
        #endif
        
        void * vaddr = ioremap((unsigned long)base_addr[i], PG_SIZE);
        
        base_addr[i] = (uint64_t) vaddr;

        #ifdef PRI
        printk("Base address for tile=%d is %llx\n", i, base_addr[i]);
        #endif
    }

    __sync_synchronize();

    return tiles;
}

uint64_t dealloc_tile(uint64_t * base, uint64_t * mmub) {
    PRINTBT
    uint64_t i;

    printk("Current num of tiles: %d\n", num_tiles);
    printk("Current base: %llx\n",  sizeof(base));
    printk("Current mmub: %llx\n",  sizeof(mmub));

    for (i = 0; i < num_tiles; i++){
        // printk("Before release mem\n");
        // release_mem_region((unsigned long)base, PG_SIZE);
        // release_mem_region((unsigned long)mmub, PG_SIZE);
        // printk("After release mem\n");
        iounmap(base[i]);
        iounmap(mmub[i]);

        #ifdef PRI
        printk("Tile %d's page successfully released!\n", i);
        #endif
    }

    return 0;
}

#endif


#ifndef DCP_ALLOC_H
#define DCP_ALLOC_H

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

    for (i = 0; i < tiles; i++){
        #ifdef PRI
        printk("Physical address is %llx\n", (unsigned long)base_addr[i]);
        #endif

        res = request_mem_region((unsigned long)base_addr[i], PG_SIZE, DRIVER_NAME);

        if (res == NULL){
            printk("[ERROR] Going to return!\n");

            return i;
        }
        
        void * vaddr = ioremap((unsigned long)base_addr[i], PG_SIZE);
        
        base_addr[i] = (uint64_t) vaddr;

        #ifdef PRI
        printk("Base address for tile=%d is %llx\n", i, base_addr[i]);
        #endif
    }

    return tiles;
}

uint64_t dealloc_tiles(void) {
    PRINTBT
    uint64_t i;

    uint32_t tileno;

    for (i = 0; i < num_tiles; i++){
        tileno = i + 2;
        release_mem_region((BASE_MAPLE | ((tileno%WIDTH) << TILE_X) | ((tileno/WIDTH) << TILE_Y)), PG_SIZE);
        release_mem_region((BASE_MMU | ((tileno%WIDTH) << TILE_X) | ((tileno/WIDTH) << TILE_Y)), PG_SIZE);
        release_mem_region((BASE_DREAM | ((tileno%WIDTH) << TILE_X) | ((tileno/WIDTH) << TILE_Y)), PG_SIZE);

        iounmap(base[i]);
        iounmap(mmub[i]);
        iounmap(dream_base[i]);
    }

    return 0;
}

#endif // DCP_ALLOC_H

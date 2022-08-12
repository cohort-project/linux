/*
 * Copyright (C) 2010-2012 Advanced Micro Devices, Inc.
 * Author: Joerg Roedel <jroedel@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#define DRIVER_NAME "cohort_mmu"
#define MAX_COHORT_TILE 2

#include "cohort_mmu.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Nazerke Turtayeva <nturtayeva@ucsb.edu>");

/////////////
// Structs //
/////////////

static int irq[MAX_COHORT_TILE];
static struct mm_struct *curr_mm;
static const struct mmu_notifier_ops iommu_mn = {
	.invalidate_range       = dec_flush_tlb,
};

static struct mmu_notifier mn = {
	.ops = &iommu_mn,
}; 

//////////////////////////
// Function Definitions //
//////////////////////////

// --> can we get a tile number from these codes?
// --> tile arguments are fixed, change this later
static irqreturn_t cohort_mmu_interrupt(int irq, void *dev_id){
	PRINTBT
	pr_info("Cohort MMU Interrupt Entered!\n");
	uint64_t res = dec_get_tlb_fault(0);
    pr_info("Get Page Fault %llx and show irq %d\n", res, irq);
    
	if (res != 0){
        uint32_t * ptr = (uint32_t *)(res & 0xFFFFFFFFF0);
        printk("T%llx\n",ptr);
        dec_resolve_page_fault(0, (res & 0xF));
        printk("R\n",res);
		return IRQ_HANDLED;
    } else{
		// --> check for the right error message
		return -1;
	}	
};

void cohort_mn_register(uint64_t c_head, uint64_t p_head, uint64_t acc_addr){
	PRINTBT
	// Extract curr process details
	struct mm_struct *mm; 
	mm      = get_task_mm(current);
	mn.mm   = mm;
	curr_mm = mm;

	// Register current process
	int err = mmu_notifier_register(&mn, curr_mm);

	// ---> address from device tree here
	uint32_t num = 1;

	// Alloc tile and set TLB base
	init_tile(num);
 
	// Fill the fifo_ctrl_t struct members
	cohort_on(c_head, p_head, acc_addr);
	
	// Open the comm-n with DEC queues to extract useful stats
	dec_open_producer(0);
	dec_open_consumer(0);
	
}
EXPORT_SYMBOL(cohort_mn_register);

static int cohort_mmu_probe(struct platform_device *ofdev)
{	
	pr_info("---> Cohort MMU Driver Probe v1.7 entered! %s\n", ofdev->resource->name);

	struct device *dev = &ofdev->dev;
	
	dev_info(dev, "---> Cohort Device Structure extracted!\n");

	/* Get IRQ for the device */
	// struct resource *res;
	// res = platform_get_resource(ofdev, IORESOURCE_IRQ, 0);
	// if (!res) {
	// 	dev_err(dev, "---> no IRQ found\n");
	// 	return -1;
	// }

	// dev_info(dev, "---> Cohort IRQ res name: %s\n", res->name);

	uint32_t irq_cnt = of_property_count_u32_elems(dev->of_node, "interrupts");
	pr_info("IRQ Cnt: %d\n", irq_cnt);

	// --> try using this function after of_property_count_u32:
	// platform_irq_count

	int retval;

	uint32_t i;

	for (i = 0; i < irq_cnt; i++){
		irq[i] = platform_get_irq(ofdev, i);
		pr_info("IRQ line: %d\n", irq[i]);
		if (irq[i] < 0) {
			dev_err(dev, "---> no IRQ found\n");
			return -1;
		}

		// listen for interrupts for a page fault
		retval = request_irq(irq[i], cohort_mmu_interrupt, 0, dev_name(dev), dev);

		dev_info(dev, "---> Cohort IRQ return value: %d\n", retval);

		if (retval < 0){
			dev_err(dev, "---> Can't request IRQ with retval: %d\n", retval);
			return retval;
		}
	}

	dev_info(dev, "---> Cohort Probe is successfully launched!\n");

	return 0;
}

void cohort_mn_exit(void){
	PRINTBT

	// Close the queues and extract numbers
	dec_close_producer(0);
	dec_close_consumer(0);
	print_stats_fifos(1);

    cohort_off();

	int release_res = dealloc_tiles();

	if (release_res != 0){
		pr_info("Error at tile %d!\n", release_res - 1);
	}

	mmu_notifier_unregister(&mn, curr_mm);

}
EXPORT_SYMBOL(cohort_mn_exit);

static int cohort_mmu_remove(struct platform_device *ofdev){
	PRINTBT

	struct device *dev = &ofdev->dev;
	free_irq(irq, dev);
	
	return 0;
}

/* Match table for OF platform binding */
static const struct of_device_id cohort_of_match[] = {
	{ .compatible = "ucsbarchlab,cohort-0.0.a", },
	// { .compatible = "ucsbarchlab,cohort-0.0.1", },
    { /* end of list */ },
};
MODULE_DEVICE_TABLE(of, cohort_of_match);

static struct platform_driver cohort_of_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = cohort_of_match,
	},
	.probe		= cohort_mmu_probe,
	.remove		= cohort_mmu_remove,
};

module_platform_driver(cohort_of_driver);
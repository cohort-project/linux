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

#include "cohort_mmu.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Nazerke Turtayeva <nturtayeva@ucsb.edu>");

// FLUSH TLB
// void dec_flush_tlb (struct mmu_notifier *mn,
// 				                            struct mm_struct *mm,
// 				                            unsigned long start, unsigned long end); 
											
// void dec_flush_tlb(struct mmu_notifier *mn, struct mm_struct *mm);

void dec_flush_tlb (struct mmu_notifier *mn, struct mm_struct *mm,
				                            unsigned long start, unsigned long end){
   PRINTBT
   
   uint64_t res = *(volatile uint64_t*)(tlb_flush | mmub[0]); // -->confugure to include the tile smhow
   printk("Dec flush tlb results: %llx", res);
}

static int irq;

static struct mm_struct *curr_mm;

static const struct mmu_notifier_ops iommu_mn = {
	// maple API is used
	.invalidate_range       = dec_flush_tlb,
};

static struct mmu_notifier mn = {
	.ops = &iommu_mn,
}; 

// --> can we get a tile number from these codes?
// --> tile arguments are fixed, change this later
static irqreturn_t cohort_mmu_interrupt(int irq, void *dev_id){
	// maple API is used
	PRINTBT
	// --> check for the similarity
	uint64_t res = dec_get_tlb_fault(0);
    pr_info("Get TLB fault %d and show irq %d\n", res, irq);
    
	if (res != 0){
        uint32_t * ptr = (uint32_t *)(res & 0xFFFFFFFFF0);
        printk("T%p\n",ptr);
        printk("T%d",*ptr);
        dec_resolve_page_fault(0, (res & 0xF));
        printk("R",res);
        //printk("R\n");
		return IRQ_HANDLED;
    } else{
		// --> check for the right error message
		return -1;
	}
};

void cohort_mn_register(uint64_t c_head, uint64_t c_meta, uint64_t c_tail, 
			   			uint64_t p_head, uint64_t p_meta, uint64_t p_tail){
	PRINTBT
	pr_info("Cohort MMU register fun-n entered! \n");

	// Device regisration with MMU notifier
	// extract curr process details
	struct mm_struct *mm; 

	mm = get_task_mm(current);
	
	curr_mm = mm;
	mn.mm = mm;

	pr_info("Cohort MMU: register fun called! \n");
	int err = mmu_notifier_register(&mn, curr_mm);
	pr_info("Cohort MMU: register fun-n returned with status %d! \n", err);

	// Alloc tile and set TLB base
	// ---> address from device tree here
	uint32_t num = 1;

	// uint32_t init_st = init_tile(num);
	init_tile(num);

	pr_info("Cohort MMU: init fun-n returned with status! \n");

	// Fill the fifo_ctrl_t struct members
	// uint32_t cohort_st = cohort_on(c_head, c_meta, c_tail, p_tail, p_meta, p_head);
	cohort_on(c_head, c_meta, c_tail, p_head, p_meta, p_tail);

	pr_info("Cohort MMU: cohort_on fun-n returned with status! \n");

}
EXPORT_SYMBOL(cohort_mn_register);

static int cohort_mmu_probe(struct platform_device *ofdev)
{	
	pr_info("---> Cohort MMU Driver Probe v1.7 entered!\n");

	struct device *dev = &ofdev->dev;
	
	dev_info(dev, "---> Cohort Device Structure extracted!\n");

	/* Get IRQ for the device */
	struct resource *res;
	res = platform_get_resource(ofdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "---> no IRQ found\n");
		return -1;
	}

	dev_info(dev, "---> Cohort IRQ res name: %s\n", res->name);

	irq = res->start;

	// listen for interrupts for a page fault
	int retval;
	retval = request_irq(irq, cohort_mmu_interrupt, 0, dev_name(dev), dev);

	dev_info(dev, "---> Cohort IRQ return value: %d\n", retval);

	if (retval < 0){
		dev_err(dev, "---> Can't request IRQ with retval: %d\n", retval);
		return retval;
	}

	dev_info(dev, "---> Cohort Probe is successfully launched!\n");

	return 0;
}

void cohort_mn_exit(void){
	PRINTBT

	pr_info("---> Cohort MMU Driver Exit\n");

	cohort_print_debug_monitors();
    
    cohort_print_monitors();

	pr_info("---> Cohort debug and stat monitors printed above\n");

    cohort_stop_monitors();
	
    cohort_off();

	pr_info("---> Cohort and monitors stopped\n");

	int release_res = dealloc_tile(base, mmub);

	if (release_res == 0){
		pr_info("All tiles successfully released!\n");
	}else{
		pr_info("Error at tile %d!\n", release_res - 1);
	}

	mmu_notifier_unregister(&mn, curr_mm);

	pr_info("---> Cohort MMU Unregistered\n");

}
EXPORT_SYMBOL(cohort_mn_exit);

static int cohort_mmu_remove(struct platform_device *ofdev){
	PRINTBT

	struct device *dev = &ofdev->dev;
	pr_info("---> Cohort MMU Driver Remove\n");

	// pr_info("Cohort Device info(2):%s\n", dev->init_name);
	dev_info(dev, "---> Cohort Device info(2):\n");
	free_irq(irq, dev);

	dev_info(dev, "---> Cohort MMU IRQ freed and leaving!\n");
	
	return 0;
}

/* Match table for OF platform binding */
static const struct of_device_id cohort_of_match[] = {
	{ .compatible = "ucsbarchlab,cohort-0.0.a", },
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
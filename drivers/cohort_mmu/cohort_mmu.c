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
#define NUM_OF_RES 3
#include "cohort_mmu.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Nazerke Turtayeva <nturtayeva@ucsb.edu>");

/////////////
// Structs //
/////////////

static int irq[MAX_COHORT_TILE];
static int irq_cnt;
static struct mm_struct *curr_mm;
// static struct 
static const struct mmu_notifier_ops iommu_mn = {
	.invalidate_range_start = invalidate_tlb_start,
	.invalidate_range_end   = invalidate_tlb_end,
};

static struct mmu_notifier mn = {
	.ops = &iommu_mn,
}; 

//////////////////////////
// Function Definitions //
//////////////////////////

/**
 * copied from xilinx_emaclite.c
 * get_bool - Get a parameter from the OF device
 * @ofdev:	Pointer to OF device structure
 * @s:		Property to be retrieved
 *
 * This function looks for a property in the device node and returns the value
 * of the property if its found or 0 if the property is not found.
 *
 * Return:	Value of the parameter if the parameter is found, or 0 otherwise
 */
static bool get_bool(struct platform_device *ofdev, const char *s)
{
	u32 *p = (u32 *)of_get_property(ofdev->dev.of_node, s, NULL);
	printk("Actual val: %d, %llx, %d, %llx\n", p, p, *p, *p);

	if (!p) {
		dev_warn(&ofdev->dev, "Parameter %s not found, defaulting to false\n", s);
		return false;
	}

	return (bool)*p;
}

// --> can we get a tile number from these codes?
// --> tile arguments are fixed, change this later
// --> modify for the copy from user logic
static irqreturn_t cohort_mmu_interrupt(int irq_n, void *dev_id){
	PRINTBT
	pr_info("Cohort MMU Interrupt Entered!\n");
	uint64_t res = dec_get_tlb_fault(irq_n%(irq[0]));
    pr_info("Get Page Fault %llx and show irq %d at core %d\n", res, irq_n, irq_n%(irq[0]));
    
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
	pr_info("---> Cohort MMU Driver Probe v1.8 entered! %s\n", ofdev->resource->name);

	struct device *dev = &ofdev->dev;
	
	dev_info(dev, "---> Cohort Device Structure extracted!\n");

	irq_cnt = of_property_count_u32_elems(dev->of_node, "interrupts");
	// irq_cnt = get_bool(ofdev, "ucsbarchlab,number-of-cohorts");
	// ---> to check the following need to recompile the code again for more
	// uint32_t irq_cnt2 = of_property_read_u32(dev->of_node, "ucsbarchlab,number-of-cohorts", &ucsbarchlab,number-of-cohorts);
	// pr_info("New IRQ Cnt: %d\n", irq_cnt);

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

	struct resource *res; 

	// do an allocation and remap for every resource by each tile
	uint32_t j;
	for (j=0; j < irq_cnt; j++){
		res = platform_get_resource(ofdev, IORESOURCE_MEM, j*NUM_OF_RES);
		base[j] = devm_ioremap_resource(dev, res);
		pr_info("base: %llx\n", base[j]);
		if (IS_ERR(base[j])){
			return PTR_ERR(base[j]);
		}

		res = platform_get_resource(ofdev, IORESOURCE_MEM, j*NUM_OF_RES+1);
		mmub[j] = devm_ioremap_resource(dev, res);
		pr_info("mmub: %llx\n", mmub[j]);
		if (IS_ERR(mmub[j])){
			return PTR_ERR(mmub[j]);
		}

		res = platform_get_resource(ofdev, IORESOURCE_MEM, j*NUM_OF_RES+2);
		dream_base[j] = devm_ioremap_resource(dev, res);
		pr_info("dream_base: %llx\n", dream_base[j]);
		if (IS_ERR(dream_base[j])){
			return PTR_ERR(dream_base[j]);
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

	// int release_res = dealloc_tiles();

	// if (release_res != 0){
	// 	pr_info("Error at tile %d!\n", release_res - 1);
	// }

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

void assert(int condition){
    if (condition==0){
        printk("ASSERT!!\n");
    }
}

#define PRINTBT

// #ifndef PRINTBT
// #define PRINTBT printk("[KERNEL] %s\n", __func__);
// #endif

// #ifndef PRINT_ADDR
// #define PRINT_ADDR;
// #endif

// #ifndef PRI
// #define PRI;
// #endif

// #define DEC_DEBUG 1 // Do some runtime checks, but potentially slows

#define DEBUG_INIT assert(initialized);
#define DEBUG_NOT_INIT assert(!initialized);

#include "dec_decoupling.h"

//////////////////////////////////////
//// OPEN/CLOSE PRODUCER/CONSUMER////
/////////////////////////////////////

uint32_t dec_open_producer(uint64_t qid) {
  PRINTBT
  while (!initialized);

#if defined(DEC_DEBUG)
  assert(fpid[qid] == INVALID_FIFO);
#endif
#ifdef RES
  printk("inside RES\n");
  ATOMIC_OP(result, 1, add, w);
#endif
  uint32_t tile = qid/queues_per_tile;
  uint64_t fifo = base[tile] | ( (qid%queues_per_tile) << FIFO);
#ifdef PRI
  printk("fifo:%d\n",fifo);
#endif
  uint32_t res_producer_conf; 
  // connect can return 0 if queue does not exist or if it is already taken 
  do {res_producer_conf = *(volatile uint64_t*)(producer_conf_addr | fifo);
#ifdef PRI
    printk("OPROD:%d and %d\n",res_producer_conf, producer_conf_addr);
#endif
  } while (res_producer_conf == 0);
  fpid[qid] = fifo;
  return 1;
}

uint32_t dec_open_consumer(uint64_t qid) {
  PRINTBT
  while (!initialized);

#if defined(DEC_DEBUG)
  assert(fcid[qid] == INVALID_FIFO);
#endif
#ifdef RES
  ATOMIC_OP(result, 1, add, w);
#endif
  uint32_t tile = qid/queues_per_tile;
  uint64_t fifo = base[tile] | ( (qid%queues_per_tile) << FIFO);
#ifdef PRI
  printk("fifo:%d\n",fifo);
#endif
  uint32_t res_consumer_conf; 
  // connect can return 0 if queue does not exist or if it is already taken 
  do {res_consumer_conf = *(volatile uint64_t*)(consumer_conf_addr | fifo); 
#ifdef PRI
    printk("OCONS:%d and %d\n",res_consumer_conf, consumer_conf_addr);
#endif
  } while (res_consumer_conf == 0);
  fcid[qid] = fifo;
  return 1;
}

uint32_t dec_close_producer(uint64_t qid) {
  PRINTBT
  uint64_t fifo = fpid[qid];
#ifdef PRI
  printk("fifo:%d\n",fifo);
#endif
#if defined(DEC_DEBUG)
  assert(fifo !=INVALID_FIFO);
  //fpid[qid] = INVALID_FIFO;
#endif
  // close can return 0 if the queue does not exist or it is not configured by the core
  volatile uint32_t res_producer_conf = *(volatile uint64_t*)(producer_dconf_addr | fifo);
#ifdef PRI
  printk("CPROD:%d and %d\n",res_producer_conf, producer_dconf_addr);
#endif
#ifdef RES
  ATOMIC_OP(result, -1, add, w);
#endif
  return res_producer_conf;
}

uint32_t dec_close_consumer(uint64_t qid) {
  PRINTBT
  uint64_t fifo = fcid[qid];
#ifdef PRI
  printk("fifo:%d\n",fifo);
#endif
#if defined(DEC_DEBUG)
  assert(fifo !=INVALID_FIFO);
  //fcid[qid] = INVALID_FIFO;
#endif
  volatile uint32_t res_consumer_conf = *(volatile uint64_t*)(consumer_dconf_addr | fifo);
#ifdef PRI
  printk("CCONS:%d and %d\n",res_consumer_conf, consumer_dconf_addr);
#endif
#ifdef RES
  ATOMIC_OP(result, -1, add, w);
#endif
  return res_consumer_conf;
}

//STATS
uint64_t dec_fifo_stats(uint64_t qid, uint32_t stat, uint32_t* result) {
  //TODO adapt to fetch only stat (key)
  PRINTBT
  volatile uint64_t res_stat = *(volatile uint64_t*)(stats_addr | fpid[qid]);
  return res_stat;
}

void print_st(uint32_t id){
  uint64_t stat = dec_fifo_stats(id,0,0);
  stat = dec_fifo_stats(id,0,0);
  uint32_t stat_c = (uint32_t) stat;
  uint32_t stat_p = (stat >> 32);
  stat = dec_fifo_stats(id,0,0);
  stat_c += (uint32_t) stat;
  stat_p += (stat >> 32);
  if (stat_c > stat_p) stat = stat_c; else stat = stat_p;
  printk("Execution time: %d\n",(int)stat);
  stat = dec_fifo_stats(id,0,0);
}

void print_stats_fifos(uint32_t num){
  uint32_t fifo_id;
  for (fifo_id=0; fifo_id<num; fifo_id++)
  {
#ifdef PRI
  printk("Stats for FIFO %d:\n", fifo_id);
#endif
  print_st(fifo_id);    
  }
}

// TLB management
// SNOOP TLB ENTRIES
uint64_t dec_snoop_tlb_entry(uint64_t tile) {
  PRINTBT
  uint64_t res = *(volatile uint64_t *)(tlb_snoop_entry | mmub[tile]);
  return res;
}
// GET PAGE FAULTS
uint64_t dec_get_tlb_fault(uint64_t tile) {
  PRINTBT
  uint64_t res = *(volatile uint64_t *)(get_tlb_fault | mmub[tile]);
  return res;
}
// FLUSH TLB
uint64_t dec_def_flush_tlb (uint64_t tile) {
  PRINTBT
  uint64_t res = *(volatile uint64_t*)(tlb_flush | mmub[tile]);
  return res;
}

int invalidate_tlb_start (struct mmu_notifier *mn, const struct mmu_notifier_range *range){
  PRINTBT
  printk("MMU flush called at: %llx and %llx\n", range->start, range->end);
   // --> configure THIS to include the tile # smhow
  uint64_t res = *(volatile uint64_t*)(tlb_flush | mmub[0]); 
#ifdef PRI
  printk("Dec flush tlb results:\n %llx", res);
#endif
  return 0;
}

void invalidate_tlb_end (struct mmu_notifier *mn, const struct mmu_notifier_range *range){

}

// CONFIG THE PAGE TABLE BASE OF THE TLB
void dec_set_tlb_ptbase(uint64_t tile, uint64_t conf_tlb_addr) {
  PRINTBT
// conf_tlb_addr is 28 bits, so set to bits [27:0]
// The other 36 bits of the data are as follow
//     35 disable_int
//     34 reserved
//     33:32 chipid[7],chipid[0]
//     31:28 fbits
//     27:20 ypos
//     19:12 xpos
//     11:10  type
//     9     threadid
//     8     0:level, 1:edge
//     7     0:rising, 1:falling
//     6:0   source id
  dec_def_flush_tlb(tile);
  *(volatile uint64_t*)(conf_tlb_ptbase | mmub[tile]) = (conf_tlb_addr & 0x0FFFFFFFULL) | 0x0003001020000000;
#ifdef PRI
  printk("Config MAPLE ptbase %llx\n", (uint64_t*)conf_tlb_addr);
#endif
}
// SET TLB ENTRY THRU DCP and RESOLVE PAGE FAULT if lower bits are set
void dec_set_tlb_mmpage(uint64_t tile, uint64_t conf_tlb_entry) {
  PRINTBT
  *(volatile uint64_t*)(conf_tlb_mmpage | mmub[tile]) = conf_tlb_entry; 
}
// RESOLVE PAGE FAULT, but not load entry into TLB, let PTW do it
void dec_resolve_page_fault(uint64_t tile, uint64_t conf_tlb_entry) {
  PRINTBT
  *(volatile uint64_t*)(resolve_tlb_fault | mmub[tile]) = conf_tlb_entry; 
}
// DISABLE TLB TRANSLATION
void dec_disable_tlb(uint64_t tile) {
  PRINTBT
  *(volatile uint64_t*)(disable_tlb | mmub[tile]) = 0; 
}

//CLEANUP
uint32_t dec_fifo_cleanup(uint32_t tile) {
  PRINTBT
#if defined(DEC_DEBUG)
  //DEBUG_INIT;
#endif
  uint32_t i;
  for (i=0; i<tile;i++){
    volatile uint32_t res_reset = *(volatile uint32_t*)(destroy_tile_addr | base[i]);
#ifdef PRI
    printk("RESET:%d\n",res_reset);
#endif
  }
  return 1; //can this fail? security issues?
}

/////////////////
/// INIT TILE ///
/////////////////

uint32_t config_loop_unit(int config_tiles, void * A, void * B, uint32_t op) {
  uint64_t cr_conf_A = op*BYTE;
  // The last bit of cr_conf_A indicates whether the B[i] load is coherent or not (1: coherent)
  // Whether A[B[i]] is loaded coherently, depends on the 'op'
  // LOOP_TLOAD32 48 (non coherent), vs  LOOP_LLC_32  52 (coherent)
#ifndef COHERENT
  #define COHERENT 1
#endif

#if COHERENT==1
  cr_conf_A |= 1 << FIFO;
#endif

  if (A!=0 || B!=0){
    int j;
    for (j=0; j<config_tiles; j++){
      *(volatile uint64_t*)(cr_conf_A | base[j]) = (uint64_t)A;
      if (B!=0) *(volatile uint64_t*)(base_addr32 | base[j]) = (uint64_t)B;
    }
  }
}

void init_tile(uint32_t num){
  PRINTBT
  uint32_t size = DCP_SIZE_64;
  if (num == 2) 
    size=DCP_SIZE_64;
  else if (num > 2)
    size=DCP_SIZE_32;
  uint32_t res = dec_fifo_init(num, size);
}

uint32_t dec_fifo_init(uint32_t count, uint32_t size) {
  PRINTBT
#ifdef SWDAE
  dec2_fifo_init(1, DCP_SIZE_64);
#endif
  return dec_fifo_init_conf(count, size, 0, 0, 0);
}

// changing the return type of the function to include errors from uint32_t
int32_t dec_fifo_init_conf(uint32_t count, uint32_t size, void * A, void * B, uint32_t op) {
  PRINTBT
#if defined(DEC_DEBUG)
  // hardware should also ignore init messages once to a tile which is already initialized
  DEBUG_NOT_INIT;
  uint32_t i;
  for(i=0; i<MAX_QUEUES;i++){
    fcid[i] = INVALID_FIFO;
    fpid[i] = INVALID_FIFO;
  }
  assert (count); //check that count is bigger than 0
#endif
  
  uint32_t allocated_tiles = count;
  uint64_t conf_tlb_addr;

  // save page table base address
	uint64_t virt_base = (uint64_t)(current->mm->pgd);

#ifdef PRI
	printk("Cohort MMU: PT base address %llx\n", virt_base);
#endif

  conf_tlb_addr = virt_to_phys((void *)virt_base) >> 12; 

#ifdef PRI
  printk("Cohort MMU: conf tlb addr %llx\n", conf_tlb_addr);
#endif
  
  if (conf_tlb_addr == -1 ) {
    return -1;
  }

  //printDebug(dec_fifo_debug(0,2));
  // dec_fifo_cleanup(allocated_tiles);
  __sync_synchronize(); //Compiler should not reorder the next load
  //printDebug(dec_fifo_debug(0,2));
  uint32_t k = 0;
  uint32_t res_producer_conf;
  do { // Do the best allocation based on the number of tiles!
    // INIT_TILE: Target to allocate "len" queues per Tile
//     uint64_t addr_maple = (base[k] | (size << FIFO));
//     res_producer_conf = *(volatile uint32_t*)addr_maple;
// #ifdef PRI
//     printk("Target MAPLE %d: address %llx\n", k, (uint32_t*)addr_maple);
//     printk("Target MAPLE %d: res %d, now config TLB\n", k, res_producer_conf);
// #endif

    dec_set_tlb_ptbase(k, conf_tlb_addr);

    k++;
  } while (k < allocated_tiles);
  // count configured tiles
  uint32_t config_tiles = k;
  // if (!res_producer_conf) config_tiles--;

  // config_loop_unit(config_tiles,A,B,op);

  initialized = 1;
  uint32_t res = config_tiles*queues_per_tile;

#ifdef PRI
  printk("INIT: res 0x%08x\n", ((uint32_t)res) & 0xFFFFFFFF);
#endif

  return res;
}
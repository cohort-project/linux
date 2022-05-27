
void assert(int condition){
    if (condition==0){
        printk("ASSERT!!\n");
    }
}

#ifndef PRI
#define PRI
#define PRINTBT printk("[PRI]%s\n", __func__);
//#define PRI_DEBUG
#endif

// #define PRINTBT

//#define DEC_DEBUG 1 // Do some runtime checks, but potentially slows
#define DEBUG_INIT assert(initialized);
#define DEBUG_NOT_INIT assert(!initialized);

#define BYTE         8
#define TILE_X       28
#define TILE_Y       24
#define WIDTH        4
#define FIFO         9
#define BASE_MAPLE 0xe100800000
#define BASE_SPD   0xe100900000
#define BASE_MMU   0xe100A00000
#define BASE_DREAM 0xe100B00000
#define BASE_NIBBL 0xe100C00000

#include "dec_decoupling.h"

#define MAX_TILES 16
#define INVALID_FIFO 65536

//static variables
static uint32_t num_tiles;
static volatile uint32_t initialized = 0;
static uint32_t queues_per_tile;
static uint64_t fpid[MAX_QUEUES];
static uint64_t fcid[MAX_QUEUES];
static uint64_t base[MAX_TILES];
static uint64_t mmub[MAX_TILES];
volatile static int result = 0;

#include "dcp_alloc.h"

#define DCP_SIZE_8  0
#define DCP_SIZE_16 1
#define DCP_SIZE_32 2
#define DCP_SIZE_48 3
#define DCP_SIZE_64 4
#define DCP_SIZE_80 5
#define DCP_SIZE_96 6
#define DCP_SIZE_128 7

///LIMA
#define LOOP_TLOAD32 48 // Set Tload 32 bits
#define LOOP_TLOAD64 49 // Set Tload 64 bits
#define LOOP_PRODUCE 50 // Set Produce
#define LOOP_LLC_32  52 // Set LLC load 32 bits
#define LOOP_LLC_64  53 // Set LLC load 64 bits
#define LOOP_PREFETCH 54 // Set Prefetch
static uint64_t push_loop = (uint64_t)(14*BYTE);

//CONSUME/PRODUCE
static uint64_t cons_addr = (uint64_t)(7*BYTE);
static uint64_t prod_addr = (uint64_t)(8*BYTE);
//TLOAD
static uint64_t tload_addr32 = (uint64_t)(10*BYTE);
static uint64_t tload_addr64 = (uint64_t)(11*BYTE);
//ATOMICs
static uint64_t add_addr  = (uint64_t)(32*BYTE);
static uint64_t and_addr  = (uint64_t)(33*BYTE);
static uint64_t or_addr   = (uint64_t)(34*BYTE);
static uint64_t xor_addr  = (uint64_t)(35*BYTE);
static uint64_t max_addr  = (uint64_t)(36*BYTE);
static uint64_t maxu_addr = (uint64_t)(37*BYTE);
static uint64_t min_addr  = (uint64_t)(38*BYTE);
static uint64_t minu_addr = (uint64_t)(39*BYTE);
static uint64_t swap_addr = (uint64_t)(40*BYTE);
static uint64_t cas1_addr = (uint64_t)(41*BYTE);
static uint64_t cas2_addr = (uint64_t)(42*BYTE);
static uint64_t prefetch_addr = (uint64_t)(43*BYTE);
static uint64_t llcload_addr32 = (uint64_t)(44*BYTE);
static uint64_t llcload_addr64 = (uint64_t)(45*BYTE);

//CONFIG
static uint64_t destroy_tile_addr  = (uint64_t)(1*BYTE);
//CONFIG B base
static uint64_t base_addr32 = (uint64_t)(57*BYTE);
static uint64_t base_addr64 = (uint64_t)(58*BYTE);

//UNUSED
static uint64_t fifoadd_addr  = (uint64_t)(6*BYTE);
static uint64_t fifoclr_addr  = (uint64_t)(9*BYTE);
static uint64_t stats_addr    = (uint64_t)(12*BYTE);
static uint64_t debug_addr    = (uint64_t)(13*BYTE);

//CONF TLB (only on the MMU page)
static uint64_t tlb_flush            = (uint64_t)(13*BYTE);
static uint64_t get_tlb_fault        = (uint64_t)(13*BYTE | 1 << FIFO);
static uint64_t tlb_snoop_entry      = (uint64_t)(13*BYTE | 2 << FIFO);
static uint64_t disable_tlb          = (uint64_t)(15*BYTE);
static uint64_t conf_tlb_ptbase      = (uint64_t)(15*BYTE | 1 << FIFO);
static uint64_t resolve_tlb_fault    = (uint64_t)(15*BYTE | 2 << FIFO);
static uint64_t conf_tlb_mmpage      = (uint64_t)(15*BYTE | 6 << FIFO);

//CONNECT
static uint64_t producer_conf_addr  = (uint64_t)(2*BYTE);
static uint64_t consumer_conf_addr  = (uint64_t)(3*BYTE);
static uint64_t producer_dconf_addr = (uint64_t)(4*BYTE);
static uint64_t consumer_dconf_addr = (uint64_t)(5*BYTE);

uint32_t dec_fifo_cleanup(uint32_t tile); 
// void printDebug(uint64_t s);
// uint64_t dec_fifo_debug(uint64_t tile, uint32_t id);

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
// // FLUSH TLB
uint64_t dec_def_flush_tlb (uint64_t tile) {
  PRINTBT
   uint64_t res = *(volatile uint64_t*)(tlb_flush | mmub[tile]);
   return res;
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

///////////////////
/// INIT TILE////
//////////////////

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

  switch(size)
    { /* 32x4, 64x2, 128x1 are the ones that make the most sense*/
        case DCP_SIZE_8:
        case DCP_SIZE_16:
        case DCP_SIZE_32: 
            queues_per_tile = 4;
            break;
        
        case DCP_SIZE_48: 
        case DCP_SIZE_64: 
            queues_per_tile = 2;
            break;
        /*
        case DCP_SIZE_80: 
        case DCP_SIZE_96:
        case DCP_SIZE_128: 
            queues_per_tile = 1;
            queues_per_tile = 1;
            break;
        */
        default:
            queues_per_tile = 1;
    }

  uint32_t partial_tile = (count%queues_per_tile)>0;
  uint32_t entire_tiles = count/queues_per_tile;
  // uint32_t num_tiles = entire_tiles + partial_tile;
  // ---> This should ideally be modified to the manycore case 
  num_tiles = entire_tiles + partial_tile;

  uint32_t allocated_tiles;
  uint64_t conf_tlb_addr;

  uint32_t tileno;
  // SET THE LAYOUT OF TILES TO TARGET

  for ( i = 0; i < num_tiles; i++){
    tileno = (i/WIDTH)*2+1;
    base[i] = BASE_MAPLE | ((tileno%WIDTH) << TILE_X) | ((0) << TILE_Y);
    mmub[i] = BASE_MMU   | ((tileno%WIDTH) << TILE_X) | ((0) << TILE_Y); 
  }
  allocated_tiles = num_tiles;

  #ifdef PRI
  printk("Num of tiles is: %d\n", num_tiles);
  #endif

  // ALLOCATE AND RESET TILE
  allocated_tiles = (uint64_t)alloc_tile(num_tiles,base);
  allocated_tiles = (uint64_t)alloc_tile(num_tiles,mmub);
  if (!allocated_tiles) return 0;
  // IF VIRTUAL MEMORY, THEN GET THE PAGE TABLE BASE
  
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
  dec_fifo_cleanup(allocated_tiles);
  __sync_synchronize(); //Compiler should not reorder the next load
  //printDebug(dec_fifo_debug(0,2));
  uint32_t k = 0;
  uint32_t res_producer_conf;
  do { // Do the best allocation based on the number of tiles!
    // INIT_TILE: Target to allocate "len" queues per Tile
    uint64_t addr_maple = (base[k] | (size << FIFO));
    res_producer_conf = *(volatile uint32_t*)addr_maple;
    #ifdef PRI
    printk("Target MAPLE %d: address %llx\n", k,(uint32_t*)addr_maple);
    printk("Target MAPLE %d: res %d, now config TLB\n", k, res_producer_conf);
    #endif

    dec_set_tlb_ptbase(k,conf_tlb_addr);

    k++;
  } while (k<allocated_tiles && res_producer_conf > 0);
  // count configured tiles
  uint32_t config_tiles = k;
  if (!res_producer_conf) config_tiles--;

  uint64_t cr_conf_A = op*BYTE | 1 << FIFO;
  int j;
  if (A!=0) for ( j=0; j<config_tiles; j++){
    *(volatile uint64_t*)(cr_conf_A | base[j]) = (uint64_t)A;
    if (B!=0) *(volatile uint64_t*)(base_addr32 | base[j]) = (uint64_t)B;
  }
  //printDebug(dec_fifo_debug(0,2));

  initialized = 1;
  uint32_t res = config_tiles*queues_per_tile;
  #ifdef PRI
  printk("INIT: res 0x%08x\n", ((uint32_t)res) & 0xFFFFFFFF);
  #endif
  return res;
}

uint32_t dec_fifo_init(uint32_t count, uint32_t size) {
  PRINTBT
#ifdef SWDAE
    dec2_fifo_init(1,DCP_SIZE_64);
#endif
    return dec_fifo_init_conf(count, size, 0, 0, 0);
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

void init_tile(uint32_t num){
    PRINTBT
    uint32_t size = DCP_SIZE_64;
    if (num == 2) 
        size=DCP_SIZE_64;
    else if (num > 2)
        size=DCP_SIZE_32;
    uint32_t res = dec_fifo_init(num, size);
}
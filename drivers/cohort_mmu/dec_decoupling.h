// #include <stdint.h>

#define QUEUE_SIZE 128
#define MAX_QUEUES 256
#define MAX_TILES 16
#define INVALID_FIFO 65536

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

//static variables
static uint32_t num_tiles;
static volatile uint32_t initialized = 0;
static uint32_t queues_per_tile;
static uint64_t fpid[MAX_QUEUES];
static uint64_t fcid[MAX_QUEUES];
static uint64_t base[MAX_TILES];
static uint64_t mmub[MAX_TILES];
static uint64_t dream_base[MAX_TILES];
volatile static int result = 0;

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

// Function declarations
uint32_t dec_open_producer(uint64_t qid);
uint32_t dec_open_consumer(uint64_t qid);
uint32_t dec_close_producer(uint64_t qid);
uint32_t dec_close_consumer(uint64_t qid);

uint64_t dec_fifo_stats(uint64_t qid, uint32_t stat, uint32_t* result);
void print_st(uint32_t id);
void print_stats_fifos(uint32_t num);

uint64_t dec_snoop_tlb_entry(uint64_t tile);
uint64_t dec_get_tlb_fault(uint64_t tile);
uint64_t dec_def_flush_tlb (uint64_t tile);
void dec_flush_tlb (struct mmu_notifier *mn, struct mm_struct *mm,
				                            unsigned long start, unsigned long end);

void dec_set_tlb_mmpage(uint64_t tile, uint64_t conf_tlb_entry);
void dec_set_tlb_mmpage(uint64_t tile, uint64_t conf_tlb_entry);
void dec_resolve_page_fault(uint64_t tile, uint64_t conf_tlb_entry);
void dec_disable_tlb(uint64_t tile);
uint32_t dec_fifo_cleanup(uint32_t tile); 

uint32_t config_loop_unit(int config_tiles, void * A, void * B, uint32_t op);

void init_tile(uint32_t num);
uint32_t dec_fifo_init(uint32_t count, uint32_t size);
int32_t dec_fifo_init_conf(uint32_t count, uint32_t size, void * A, void * B, uint32_t op);

typedef uint64_t addr_t; // though we only use the lower 32 bits
typedef uint32_t len_t; // length of fifo
typedef len_t ptr_t;
typedef uint32_t el_size_t; // element size width

struct _fifo_ctrl_t;

typedef struct _fifo_ctrl_t fifo_ctrl_t;

typedef struct __attribute__((__packed__)) {
    addr_t addr;
    el_size_t size;
    len_t len;
    char unused [112];
    char acc_addr [128]; 
} meta_t;

typedef void (*fifo_push_func_t)(uint64_t element, fifo_ctrl_t* fifo_ctrl);
typedef uint64_t (*fifo_pop_func_t)(fifo_ctrl_t* fifo_ctrl);

struct _fifo_ctrl_t {
    uint32_t fifo_length;
    uint32_t element_size;
    ptr_t* head_ptr;
    ptr_t* tail_ptr;
    meta_t* meta_ptr;
    void* data_array;
    
    // function pointers to actual implemented function
    // decided at class instantiation time
    fifo_push_func_t fifo_push_func;
    fifo_pop_func_t fifo_pop_func;
    
};

extern void cohort_mn_register(uint64_t c_head, uint64_t p_head, uint64_t acc_addr);

extern void cohort_mn_exit(void);
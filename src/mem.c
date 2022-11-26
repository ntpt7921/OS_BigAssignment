#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];
static uint32_t free_frame_left = NUM_PAGES;
static const uint32_t max_virtual_addr = ~(~0u << ADDRESS_SIZE);

static struct {
    uint32_t proc;	// ID of process currently uses this page
    int index;	// Index of the page in the list of pages allocated to the process.
    int next;	// The next page in the list. -1 if it is the last page.
} _mem_stat [NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void) {
    memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
    memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
    pthread_mutex_init(&mem_lock, NULL);
}

void init_segment_table(struct seg_table_t *s_table)
{
    s_table->size = 0;
    for (uint32_t i = 0; i < (1u << SEGMENT_LEN); i++)
    {
        // write null to all entry's page table
        s_table->table[i].pages = NULL; // considered not initialized
    }
}

void init_page_table(struct page_table_t *p_table)
{
    p_table->size = 0;
    for (uint32_t i = 0; i < (1u << PAGE_LEN); i++)
    {
        // write 0 to v_index for each entry
        p_table->table[i].v_index = 0;
    }
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
    return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
    return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
    return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

static void set_page_table_entry(struct pcb_t *proc, uint32_t page_num, uint32_t virt_addr)
{
    // page num is the ten most significant bit within the address
    // it contain the segment index (5 bit) and page index (5 bit)
    uint32_t segment_index = get_first_lv(virt_addr);
    uint32_t page_index = get_second_lv(virt_addr);
    struct seg_table_t *stab = proc->seg_table;
    struct page_table_t **ptab = &(stab->table[segment_index].pages);

    if (*ptab == NULL)
    {
        // not allocated yet
        *ptab = (struct page_table_t*) malloc(sizeof(struct page_table_t));
        init_page_table(*ptab);
        stab->size++;
    }

    if ((*ptab)->table[page_index].v_index == 0)
    {
        // page has not been set to anyone yet
        (*ptab)->table[page_index].v_index = virt_addr >> OFFSET_LEN;
        (*ptab)->table[page_index].p_index = page_num;
        (*ptab)->size++;
    }
}

static void free_page_table_entry(addr_t addr, struct pcb_t *proc)
{
    uint32_t segment_index = get_first_lv(addr);
    uint32_t page_index = get_second_lv(addr);
    struct seg_table_t *stab = proc->seg_table;
    struct page_table_t *ptab = stab->table[segment_index].pages;

    ptab->table[page_index].v_index = 0;
    ptab->size--;
}

/* Search for page table table from the a segment table */
static struct page_table_t * get_page_table(
        addr_t index, 	// Segment level index
        struct seg_table_t * seg_table) { // first level table

    /*
     * TODO: Given the Segment index [index], you must go through each
     * row of the segment table [seg_table] and check if the v_index
     * field of the row is equal to the index
     *
     * */

    return seg_table->table[index].pages;
}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
        addr_t virtual_addr, 	// Given virtual address
        addr_t * physical_addr, // Physical address to be returned
        struct pcb_t * proc) {  // Process uses given virtual address

    /* The first layer index */
    addr_t first_lv = get_first_lv(virtual_addr);
    /* The second layer index */
    addr_t second_lv = get_second_lv(virtual_addr);

    /* Search in the first level */
    struct page_table_t * page_table =
        get_page_table(first_lv, proc->seg_table);

    if (page_table == NULL)
    {
        // page table for specified index does not exist
        return 0;
    }

    if (page_table->table[second_lv].v_index == 0)
    {
        // page is not used
        return 0;
    }

    /* Offset of the virtual address */
    addr_t offset = get_offset(virtual_addr);
    addr_t physical_frame_index = page_table->table[second_lv].p_index;
    *physical_addr = (physical_frame_index << OFFSET_LEN) | offset;
    return 1;
}

addr_t alloc_mem(uint32_t size, struct pcb_t * proc) {
    pthread_mutex_lock(&mem_lock);
    addr_t ret_mem = 0;
    /* TODO: Allocate [size] byte in the memory for the
     * process [proc] and save the address of the first
     * byte in the allocated memory region to [ret_mem].
     * */


    // add one more page for the remainer (causing intermal frag)
    uint32_t num_pages = size / PAGE_SIZE +
        ((size % PAGE_LEN) ? 1 : 0);
    int mem_avail = 1; // We could allocate new memory region or not?

    /* First we must check if the amount of free memory in
     * virtual address space and physical address space is
     * large enough to represent the amount of required
     * memory. If so, set 1 to [mem_avail].
     * Hint: check [proc] bit in each page of _mem_stat
     * to know whether this page has been used by a process.
     * For virtual memory space, check bp (break pointer).
     * */

    // check physical mem for free frame left
    if (free_frame_left < num_pages)
        mem_avail = 0;
    // check virtual mem for free pages left
    uint32_t required_alloc_size = num_pages * PAGE_SIZE;
    if (proc->bp + required_alloc_size > max_virtual_addr)
        mem_avail = 0;

    if (mem_avail) {
        /* We could allocate new memory region to the process */
        ret_mem = proc->bp;
        /* Update status of physical pages which will be allocated
         * to [proc] in _mem_stat. Tasks to do:
         * 	- Update [proc], [index], and [next] field
         * 	- Add entries to segment table page tables of [proc]
         * 	  to ensure accesses to allocated memory slot is
         * 	  valid. */

        // loop through _mem_stat, find free frame
        // if free frame found, update status for that fram
        // also update the page table
        // stop when all required pages have its associated frame
        uint32_t alloc_frame_count = 0;
        uint32_t last_frame_index;
        uint8_t is_first_frame_encountered = 1;
        for (uint32_t i = 0; alloc_frame_count < num_pages && i < NUM_PAGES;
                i++)
        {
            if (_mem_stat[i].proc == 0)
            {
                _mem_stat[i].proc = proc->pid;
                _mem_stat[i].index = alloc_frame_count++;

                if (is_first_frame_encountered)
                    is_first_frame_encountered = 0;
                else
                    _mem_stat[last_frame_index].next = i;

                last_frame_index = i;

                set_page_table_entry(proc, i, proc->bp);
                proc->bp += PAGE_SIZE;
            }
        }
        _mem_stat[last_frame_index].next = -1;


        if (alloc_frame_count < num_pages)
        {
            // this is a error
            fprintf(stderr, "Error allocating pages, process finish but there"
                    "are still pages to allocate");
            while (1); // stop here
        }

        free_frame_left -= num_pages;
    }
    pthread_mutex_unlock(&mem_lock);
    return ret_mem;
}

int free_mem(addr_t address, struct pcb_t * proc) {
    /*TODO: Release memory region allocated by [proc]. The first byte of
     * this region is indicated by [address]. Task to do:
     * 	- Set flag [proc] of physical page use by the memory block
     * 	  back to zero to indicate that it is free.
     * 	- Remove unused entries in segment table and page tables of
     * 	  the process [proc].
     * 	- Remember to use lock to protect the memory from other
     * 	  processes.  */

    // Since the virtual memory address can be stored in cpu register
    // (without any way to update it), it is assume that we can not do
    // anything about fragmentation within the virtual address space.
    //
    // So freeing memory will not update the break pointer (proc->bp)
    pthread_mutex_lock(&mem_lock);
    while (1)
    {
        // get the frame associated with address
        uint32_t physical_addr;
        translate(address, &physical_addr, proc);
        uint32_t current_frame_index = physical_addr >> OFFSET_LEN;

        // set frame status to not allocated
        _mem_stat[current_frame_index].proc = 0;

        free_page_table_entry(address, proc);

        if (_mem_stat[current_frame_index].next == -1)
            break;

        address += PAGE_SIZE;
        if (address > max_virtual_addr)
        {
            // error
            fprintf(stderr, "Error freeing memory, "
                    "memory region span over the max virtual address boundary\n");
            while (1); // stop here
        }
    }
    pthread_mutex_unlock(&mem_lock);
    return 0;
}

int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
    addr_t physical_addr;
    if (translate(address, &physical_addr, proc)) {
        *data = _ram[physical_addr];
        return 0;
    }else{
        return 1;
    }
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
    addr_t physical_addr;
    if (translate(address, &physical_addr, proc)) {
        _ram[physical_addr] = data;
        return 0;
    }else{
        return 1;
    }
}

void dump(void) {
    int i;
    for (i = 0; i < NUM_PAGES; i++) {
        if (_mem_stat[i].proc != 0) {
            printf("%03d: ", i);
            printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
                    i << OFFSET_LEN,
                    ((i + 1) << OFFSET_LEN) - 1,
                    _mem_stat[i].proc,
                    _mem_stat[i].index,
                    _mem_stat[i].next
                  );
            int j;
            for (	j = i << OFFSET_LEN;
                    j < ((i+1) << OFFSET_LEN) - 1;
                    j++) {

                if (_ram[j] != 0) {
                    printf("\t%05x: %02x\n", j, _ram[j]);
                }

            }
        }
    }
}



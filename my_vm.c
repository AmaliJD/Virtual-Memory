#include "my_vm.h"

int off_bits = 0, mid_bits = 0, front_bits = 0;
/*
Function responsible for allocating and setting your physical memory
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    int num_pages = MEMSIZE / PGSIZE;
    int num_entries_per_page = PGSIZE / sizeof(pte_t);
    double ob = log10(PGSIZE) / log10(2);
    off_bits = (int)ceil(ob);
    mid_bits = (32 - off_bits) / 2;
    front_bits = front_bits;

    ppage_count = 1 << mid_bits;
    vpage_count = 1 << front_bits;

    //not sure what type to set physical mem to so i have it is
    physical_mem = (unsigned char*)malloc(MEMSIZE);
    page_dir = (pde_t*)malloc(page_count);
    vbitmap = (valid_bit*)malloc(page_count);
    pbitmap = (valid_bit*)malloc(vpage_count);
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them

    tlb_store.miss_count = 0;
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int
add_TLB(void* va, pte_t* pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    pthread_mutex_lock(&tlb_lock);
    tlb_store.miss_count += 1;

    int i = 0;
    while (tlb_store.page_dir_nums[i] != NULL)
    {
        i++;
        if (i >= TLB_ENTRIES) { break; }
    }

    unsigned int entry_value = get_top_bits((unsigned int)va, front_bits + mid_bits);
    unsigned int pa_value = (unsigned int)pa;

    tlb_store.page_dir_nums[i] = entry_value;
    tlb_store.physical_addrs[i] = pa_value;
    pthread_mutex_unlock(&tlb_lock);

    return 1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t*
check_TLB(void* va) {

    /* Part 2: TLB lookup code here */
    pthread_mutex_lock(&tlb_lock);
    unsigned int entry_value = get_top_bits((unsigned int)va, front_bits + mid_bits);

    int i = 0;

    while (tlb_store.page_dir_nums[i] != entry_value)
    {
        i++;
        if (i >= TLB_ENTRIES)
        {
            return NULL;
        }
    }

    pte_t* pa_value = tlb_store.physical_addrs[i];
    pthread_mutex_unlock(&tlb_lock);

    return pa_value;
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0;
    /*Part 2 Code here to calculate and print the TLB miss rate*/
    miss_rate = tlb_store.miss_count / tlb_store.mem_accesses;

    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}

/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t* translate(pde_t* pgdir, void* va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */

    // Translation
    /*
        Go to pgdir. Offset by vpn to get pagetable address
        Go to pagetable address. Offset by ppn to get page address
        Go to page address. Offset by off to get data address
        Go to data address -> retrieve value
    */

    // check TLB first
    pte_t* paddr = check_TLB(va);

    if (paddr == NULL)
    {
        unsigned int vaddr = (unsigned int)va;
        unsigned int vpn = get_top_bits(vaddr, front_bits);
        unsigned int ppn = get_mid_bits(vaddr, mid_bits, off_bits);
        unsigned int off = get_end_bits(vaddr, off_bits);

        printf("PGDIR: %lx\n", pgdir);
        pte_t* outer = pgdir[vpn];
        printf("OUTER: %lx\n", outer);
        pte_t* inner = outer[ppn];
        paddr = &inner[off];

        add_TLB(va, paddr);
    }

    return paddr;

    //If translation not successfull
    //return NULL;
}




// EXTRACT BITS HELPER --------------------------------------------------------------------------------------------------
// Example 1 EXTRACTING TOP-BITS (Outer bits)

unsigned int get_top_bits(unsigned int value, int num_bits)
{
    //Assume you would require just the higher order (outer)  bits, 
    //that is first few bits from a number (e.g., virtual address) 
    //So given an  unsigned int value, to extract just the higher order (outer)  ?num_bits?
    int num_bits_to_prune = 32 - num_bits; //32 assuming we are using 32-bit address 
    return (value >> num_bits_to_prune);
}


//Example 2 EXTRACTING BITS FROM THE MIDDLE
//Now to extract some bits from the middle from a 32 bit number, 
//assuming you know the number of lower_bits (for example, offset bits in a virtual address)

unsigned int get_mid_bits(unsigned int value, int num_middle_bits, int num_lower_bits)
{

    //value corresponding to middle order bits we will returning.
    unsigned int mid_bits_value = 0;

    // First you need to remove the lower order bits (e.g. PAGE offset bits). 
    value = value >> num_lower_bits;


    // Next, you need to build a mask to prune the outer bits. How do we build a mask?   

    // Step1: First, take a power of 2 for ?num_middle_bits?  or simply,  a left shift of number 1.  
    // You could try this in your calculator too.
    unsigned int outer_bits_mask = (1 << num_middle_bits);

    // Step 2: Now subtract 1, which would set a total of  ?num_middle_bits?  to 1 
    outer_bits_mask = outer_bits_mask - 1;

    // Now time to get rid of the outer bits too. Because we have already set all the bits corresponding 
    // to middle order bits to 1, simply perform an AND operation. 
    mid_bits_value = value & outer_bits_mask;

    return mid_bits_value;

}

unsigned int get_end_bits(unsigned int value, int num_bits)
{
    return (value % (1 << num_bits));
}

//Example 3
//Function to set a bit at "index"
// bitmap is a region where were store bitmap 
static void set_bit_at_index(char* bitmap, int index)
{
    // We first find the location in the bitmap array where we want to set a bit
    // Because each character can store 8 bits, using the "index", we find which 
    // location in the character array should we set the bit to.
    char* region = ((char*)bitmap) + (index / 8);

    // Now, we cannot just write one bit, but we can only write one character. 
    // So, when we set the bit, we should not distrub other bits. 
    // So, we create a mask and OR with existing values
    char bit = 1 << (index % 8);

    // just set the bit to 1. NOTE: If we want to free a bit (*bitmap_region &= ~bit;)
    *region |= bit;

    return;
}


//Example 3
//Function to get a bit at "index"
static int get_bit_at_index(char* bitmap, int index)
{
    //Same as example 3, get to the location in the character bitmap array
    char* region = ((char*)bitmap) + (index / 8);

    //Create a value mask that we are going to check if bit is set or not
    char bit = 1 << (index % 8);

    return (int)(*region >> (index % 8)) & 0x1;
}
// ---------------------------------------------------------------------------------------------------------------------


void update_pbitmap(int index){
    pthread_mutex_lock(&pbitmap_lock);
    pbitmap[index] = !pbitmap[index];
    pthread_mutex_unlock(&pbitmap_lock);
}

void update_vbitmap(int index){
    pthread_mutex_lock(&vbitmap_lock);
    vbitmap[index] = !pbitmap[index];
    pthread_mutex_unlock(&vbitmap_lock);
}

/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t* pgdir, void* va, void* pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
    pte_t addr = (pte_t)va;
    int offset_bits = (int)ceil(log2(PGSIZE));
    //int second_bits = min((32 - offset_bits) / 2, offset_bits);
    int a = (32 - offset_bits) / 2;
    int b = offset_bits;
    int second_bits = (a > b) ? a : b;
    if (off_bits == second_bits) {
        second_bits -= (int)log2(sizeof(pde_t));
    }
    long mask = (1 << second_bits) - 1;

    //insert lock call to make threadsafe
    pthread_mutex_lock(&pt_lock);
    if (!pgdir[offset_bits]) {
        pte_t* page = malloc(PGSIZE / sizeof(pte_t));
        pgdir[offset_bits] = *page;
    }

    
    ((pte_t *) pgdir[offset_bits])[second_bits] = (pte_t)pa;
    pthread_mutex_unlock(&pt_lock);
    //unlock call

    //need to check TLB and update insert checkTLB call

    return 0;
}


/*Function that gets the next available page and returns respective index*/
void* get_next_avail(int num_pages) {

    //Use virtual address bitmap to find the next free page
    /*
    logic: simply iterate thru the pagedir bitmap and find the first 0 and return starting address for that page???
    */
    int index = 0;
    int i = 0;
    int temp = num_pages;
    int* arr = malloc(page_count * sizeof(int));
    int zero = 1;
    //should we throw in a lock here??? why not
    pthread_mutex_lock(&vbitmap_lock);
    for (i = 0; i < page_count; i++) {
        if (vbitmap[i] == 0){
                //printf("0\n");
                zero = 0;
                temp--;
                arr[index] = i;
                index++;
        }
        else{
            //printf("1\n");
            temp = num_pages;
            index = 0;
            zero = 1;
        }
        if (temp == 0){
            break;
        }
    }    
    pthread_mutex_unlock(&vbitmap_lock);
    if(temp == 0){
        return (void*) arr;
    }
    else{ 
        return NULL;
    }
}

int* get_avail_phys(int count){
    int* arr = malloc(page_count * sizeof(int));
    int temp = count;
    int i;
    int index = 0;
    pthread_mutex_lock(&pbitmap_lock);
    for (i = 0; i < page_count; i++) {
        if (pbitmap[i] == 0){
            arr[index] = i;
            index++;
        }
        if (count == 0){
            break;
        }
    }    
    pthread_mutex_unlock(&pbitmap_lock);
        if (count == 0){
            return arr;
        }
        else{
            return NULL;
        }
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void* a_malloc(unsigned int num_bytes) {

    /*
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */

     /*
      * HINT: If the page directory is not initialized, then initialize the
      * page directory. Next, using get_next_avail(), check if there are free pages. If
      * free pages are available, set the bitmaps and map a new page. Note, you will
      * have to mark which physical pages are used.
      */

      /*
      logic:
      1. if num_byes <= page size: call get next_avail_page and then malloc on the pagedir val
      2. if not, see how many pages are needed
      3. return the physsical mem addr??? (check)
      */

    if (physical_mem == NULL) {
        set_physical_mem();
    }

    //next[0] = vpage number, next[1] = physical page number
    int num_pages = (int) ceil(num_bytes/PGSIZE);
    
    //getting the available CONSECUTIVE vpage entries 
    int* next_vp = get_next_avail(num_pages);
    if (next_vp == NULL) {
        return NULL;
    }
    
    //getting the available physical pages (doesn't have to be consecutive)
    int* next_pp = get_avail_phys(page_count);
    if (next_pp == NULL){
        return NULL;
    }
    //need to figure out how many entries in a page
    //page_dir[next[0]] = (pde_t*)malloc(PGSIZE);

    //having issues working out how to handle vaddrs for malloc calls that require multiple pages
    unsigned int vpn = next_vp[0]; // index of outerpage
    unsigned int ppn = next_vp[1]; // index of innerpage
    //unsigned int off = num_bytes;
    unsigned int off = 0; // assuming new page per a_malloc
    unsigned int vaddr = (vpn * (1 << 32)) + (ppn * (1 << (32 - front_bits))) + off;

    void* vpointer = vaddr;
    return vpointer;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void* va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */

     /*logic:
         1. get the translation for the physical page free that
         2. get the page dir index and then free that
         3. check if value is in TLB by using page dir index and free if necessary
     */


}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void put_value(void* va, void* val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */

     //INSERT TRANSLATE CALL - assuming it just returns the addr for the start of phys page in physical_mem
    pde_t* paddr = translate(page_dir, va);

    int index = (*paddr) * (PGSIZE - 1);
    memcpy(paddr, val, size);
}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void* va, void* val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */
    //logic: simply get the physical addr, and set va to be the dereferenced value???
    pde_t* paddr = translate(page_dir, va);
    int index = (*paddr) * (PGSIZE - 1);
    int i;
    unsigned char* temp = (char*)malloc(size);
    //not sure if this is right but i'm just setting val[i] = phys_mem[i]
    for (i = index; i < index + PGSIZE; i++) {
        temp[i] = physical_mem[i];
    }
    memcpy(temp, val, temp);
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void* mat1, void* mat2, int size, void* answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to
     * getting the values from two matrices, you will perform multiplication and
     * store the result to the "answer array"
     */


}

/* TESTING*/
int main()
{
    int* temp = a_malloc(10*sizeof(int));
    /*int i;
	for(i = 0; i < 10; i++)
		printf("0x%x\n", temp[i]);
	*/
	printf("%x\n", temp);
	printf("%lf\n", temp);
	/*
	page_count = 10;
    int i;
    for(i = 0; i < 10; i++){
        if(i >= 5 && i <=8){
            vbitmap[i] = 0;
        }
        else{
            vbitmap[i] = 1;
        }
    }

    int* arr = get_next_avail(3);
    */
	return 0;

}
/* */

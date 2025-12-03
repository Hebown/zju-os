#include "mm.h"
#include "printk.h"
#include "string.h"
#include "defs.h"
#include "virtio.h"
#include "vma.h"
#include <stdint.h>

const uint64_t V=1UL<<0;
const uint64_t R=1UL<<1;
const uint64_t W=1UL<<2;
const uint64_t X=1UL<<3;

/* early_pgtbl: 用于 setup_vm 进行 1GiB 的映射 */
uint64_t early_pgtbl[512] __attribute__((__aligned__(0x1000)));


extern void relocate();
void setup_vm() {
    /* 
     * 1. 由于是进行 1GiB 的映射，这里不需要使用多级页表 
     * 2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
     *     high bit 可以忽略
     *     中间 9 bit 作为 early_pgtbl 的 index
     *     低 30 bit 作为页内偏移，这里注意到 30 = 9 + 9 + 12，即我们只使用根页表，根页表的每个 entry 都对应 1GiB 的区域
     * 3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    **/
    const uint64_t PHYSICAL_ADDR=0x80000000UL;
    const uint64_t IDENTITY_VIRTUAL=0x80000000UL;
    const uint64_t DIRECT_MAPPING_VIRTUAL=0xffffffe000000000UL;
    
    for(int i=0;i<512;i++)early_pgtbl[i]=0;

    // 我们要做的是，根据虚拟地址和物理地址之间的关系，建立虚拟页表项，塞到early_pgtbl中

    // 虚拟地址到虚拟页表索引的转换，放在根页表。
    const uint64_t IDENTITY_INDEX=(IDENTITY_VIRTUAL>>30)&0x1ff;
    const uint64_t DIRECT_MAPPING_INDEX=(DIRECT_MAPPING_VIRTUAL>>30)&0x1ff;

    // 接下来我们要把物理页面的ppn（它是哪一页）放到根页表的对应的项中
    const uint64_t ppn=(PHYSICAL_ADDR>>30)&0x3ffffff;
    // ppn 放到页表项的最高的那个ppn2里面，同时还要添加各种flag
    const uint64_t entry_val=(ppn<<28)|V|R|W|X;

    // 然后把项的value放到表中特定位置去
    early_pgtbl[IDENTITY_INDEX]=early_pgtbl[DIRECT_MAPPING_INDEX]=entry_val;
}
void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);


/* swapper_pg_dir: kernel pagetable 根目录，在 setup_vm_final 进行映射 */
uint64_t swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

extern char _stext[], _etext[];
extern char _srodata[], _erodata[];
extern char _sdata[], _edata[];
extern char _ekernel[];

void setup_vm_final() {
    memset(swapper_pg_dir, 0x0, PGSIZE);
    
    uint64_t text_size = ((uint64_t)_etext - (uint64_t)_stext);
    uint64_t rodata_size = ((uint64_t)_erodata - (uint64_t)_srodata);
    uint64_t data_size = (uint64_t)(VM_START + PHY_SIZE) - (uint64_t)_sdata;
    
    // No OpenSBI mapping required
    
    // mapping kernel text X|-|R|V
    create_mapping(swapper_pg_dir,(uint64_t)_stext,(uint64_t)_stext-PA2VA_OFFSET,text_size,X|R|V);
    
    // mapping kernel rodata -|-|R|V
    create_mapping(swapper_pg_dir,(uint64_t)_srodata,(uint64_t)_srodata-PA2VA_OFFSET,rodata_size,R|V);
    
    // mapping other memory -|W|R|V
    create_mapping(swapper_pg_dir,(uint64_t)_sdata,(uint64_t)_sdata-PA2VA_OFFSET,data_size,W|R|V);

    // virtIO 外设初始化
    create_mapping(swapper_pg_dir, io_to_virt(VIRTIO_START), VIRTIO_START, VIRTIO_SIZE * VIRTIO_COUNT, W | R | V);

    // set satp with swapper_pg_dir

    uint64_t satp_value = (8UL << 60) | (((uint64_t)swapper_pg_dir - PA2VA_OFFSET) >> 12 );
    asm volatile("csrw satp, %0" : : "r"(satp_value));


    // flush TLB
    asm volatile("sfence.vma zero, zero");
    return;
}


/* 创建多级页表映射关系 */
/* 不要修改该接口的参数和返回值 */
void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm) {
    /*
     * pgtbl 为根页表的基地址
     * va, pa 为需要映射的虚拟地址、物理地址
     * sz 为映射的大小，单位为字节
     * perm 为映射的权限（即页表项的低 8 位）
     * 
     * 创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
     * 可以使用 V bit 来判断页表项是否存在
    **/

    // 确保按页对齐
    uint64_t start_va = va & ~(PGSIZE - 1);
    uint64_t start_pa = pa & ~(PGSIZE - 1);
    // printk("start_pa = %llx\n",start_pa);
    uint64_t mapping_size = (sz + PGSIZE - 1) & ~(PGSIZE - 1);
    uint64_t pages = mapping_size / PGSIZE;
    // printk("pages = %llu\n",pages);

    for (uint64_t i=0; i<pages; i++) {
        uint64_t current_va = start_va + i * PGSIZE;
        uint64_t current_pa = start_pa + i * PGSIZE;

        // 分解虚拟地址：Sv39模式，9+9+9+12
        uint64_t vpn2 = (current_va >> 30) & 0x1FF;  
        uint64_t vpn1 = (current_va >> 21) & 0x1FF;  
        uint64_t vpn0 = (current_va >> 12) & 0x1FF;  

        uint64_t *current_level = pgtbl;

        // 第一级页表查找 (vpn2)
        if (!(current_level[vpn2] & V)) {
            // 分配第二级页表
            uint64_t *second_level = (uint64_t*)kalloc();
            // printk("second_level = %llx\n",(uint64_t)second_level);
            if (!second_level) {
                // 处理分配失败
                printk(RED"分配二级失败！\n"CLEAR);
            }
            memset(second_level, 0, PGSIZE);
            current_level[vpn2] = (((uint64_t)second_level - PA2VA_OFFSET >> 12) << 10) | V;
        }
        current_level = (uint64_t*)(((current_level[vpn2] >> 10) << 12)+PA2VA_OFFSET);

        if (!(current_level[vpn1] & V)) {
            // 分配第三级页表
            uint64_t *third_level = (uint64_t*)kalloc();
            if (!third_level) {
                printk(RED"分配三级失败！\n"CLEAR);
            }
            memset(third_level, 0, PGSIZE);
            current_level[vpn1] = (((uint64_t)third_level - PA2VA_OFFSET >> 12) << 10) | V;
        }
        current_level = (uint64_t*)(((current_level[vpn1] >> 10) << 12)+PA2VA_OFFSET);

        // 第三级页表：设置最终的页表项
        uint64_t ppn = current_pa >> 12;
        // printk(RED"current_pa = %llx\n"CLEAR,current_pa);
        // printk(BLUE"ppn = %llx\n"CLEAR,ppn);
        // printk(GREEN"vpn = %llx\n"CLEAR,vpn0);
        current_level[vpn0] = (ppn << 10) | perm;
        // printk("current_level[vpn0] = %llx\n",current_level[vpn0]);
    }
    // printk("Mapped: va=0x%lx->0x%lx to pa=0x%lx->0x%lx, perm=0x%lx\n", 
    //        start_va, start_va + mapping_size - 1,
    //        start_pa, start_pa + mapping_size - 1, 
    //        perm);
    // printk(RED"pgtbl = %lx\n"CLEAR,pgtbl);
}

struct vm_area_struct* find_vma(struct mm_struct *mm, uint64_t addr) {
    for (struct vm_area_struct* cur = mm->mmap; cur != NULL; cur = cur->vm_next) {
        if (addr >= cur->vm_start && addr < cur->vm_end) {
            return cur;
        }
    }
    return NULL;
}

uint64_t do_mmap(struct mm_struct *mm, uint64_t addr, uint64_t len, uint64_t vm_pgoff, uint64_t vm_filesz, uint64_t flags){
    struct vm_area_struct* vma=kalloc();
    vma->vm_mm=mm;
    vma->vm_prev=NULL;
    vma->vm_next=NULL;
    vma->vm_start=addr;
    vma->vm_end=addr+len;
    vma->vm_pgoff=vm_pgoff;
    vma->vm_filesz=vm_filesz;
    vma->vm_flags=flags;
    if(mm->mmap==NULL){
        mm->mmap=vma;
    }else{
        struct vm_area_struct* head=mm->mmap;
        mm->mmap->vm_prev=vma;
        vma->vm_next=head;
        mm->mmap=vma;
    }
    return vma->vm_start;
}
#include "fs.h"
#include "mm.h"
#include "defs.h"
#include "proc.h"
#include "stdlib.h"
#include "printk.h"
#include "elf.h"
#include "vma.h"
#include <stdint.h>
extern void __dummy();
extern uint64_t swapper_pg_dir[];
extern char _sramdisk[], _eramdisk[];

extern void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);

struct task_struct *idle;           // idle process
struct task_struct *current;        // 指向当前运行线程的 task_struct
struct task_struct *task[NR_TASKS]; // 线程数组，所有的线程都保存在此
uint64_t nr_tasks;

#define SSTATUS_SPP (1L << 8)   // Previous privilege mode (1=S, 0=U)
#define SSTATUS_SPIE (1L << 5)  // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4)  // User Previous Interrupt Enable  
#define SSTATUS_SUM (1L << 18)  // Supervisor User Memory access enable

void load_program(struct task_struct *task) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)_sramdisk;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)(_sramdisk + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        Elf64_Phdr *phdr = phdrs + i;
        if (phdr->p_type == PT_LOAD) {
            uint64_t page_num = (phdr->p_memsz + PGSIZE - 1) / PGSIZE;
            char* paddr = alloc_pages(page_num);
            uint64_t *addr = (uint64_t*)(_sramdisk + phdr->p_offset);
            uint64_t phy_offset = (uint64_t)addr & (PGSIZE - 1);
            for(uint64_t i = 0; i < phdr->p_memsz; i++){
                *((char*)paddr + i + phy_offset) = *((char*)addr + i);
            }
            for(uint64_t i = phdr->p_filesz; i < phdr->p_memsz; i++){
                *((char*)paddr + i + phy_offset) = 0;
            }
            create_mapping(task->pgd, phdr->p_vaddr, (uint64_t)paddr - PA2VA_OFFSET, phdr->p_memsz, 0x1f);
        }
    }
    task->thread.sepc = ehdr->e_entry;
}

void task_init() {
    printk("run task_init\n");
    srand(2024);    
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度，可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle
    
    idle=(struct task_struct*)kalloc();
    idle->state=TASK_RUNNING;
    idle->counter=0,idle->priority=0;
    idle->pid=0;
    current=task[0]=idle;

    // 1. 参考 idle 的设置，为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，counter 和 priority 进行如下赋值：
    //     - counter  = 0;
    //     - priority = rand() 产生的随机数（控制范围在 [PRIORITY_MIN, PRIORITY_MAX] 之间）
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 thread_struct 中的 ra 和 sp
    //     - ra 设置为 __dummy（见 4.2.2）的地址
    //     - sp 设置为该线程申请的物理页的高地址
    for(int i=2;i<NR_TASKS;i++){
        task[i]=NULL;
    }
    nr_tasks=1;
    for(int i=1;i<nr_tasks+1;i++){
        task[i]=kalloc();
        task[i]->state=TASK_RUNNING;
        task[i]->counter=0,task[i]->priority=(rand()%PRIORITY_MAX)+PRIORITY_MIN;
        task[i]->pid=i;
        task[i]->thread.ra=(uint64_t)__dummy;
        task[i]->thread.sp=(uint64_t)task[i]+(uint64_t)(1024*4);

        // lab4: 配置用户态线程的上下文
        // 1. 将 sepc 设置为 USER_START
        task[i]->thread.sepc = USER_START;
        
        // 2. 配置 sstatus
        //    - 清除 SPP (使得 sret 返回至 U-Mode)
        //    - 设置 SUM (S-Mode 可以访问 User 页面)
        task[i]->thread.sstatus = 0;
        task[i]->thread.sstatus &= ~SSTATUS_SPP;  // SPP=0, 返回用户态
        task[i]->thread.sstatus |= SSTATUS_SUM;   // 允许内核访问用户内存
        
        // 3. 将 sscratch 设置为 U-Mode 的 sp (用户栈顶)
        task[i]->thread.sscratch = USER_END;

        // 4. 分配页表
        task[i]->pgd = (uint64_t*)kalloc();
        uint64_t *src = (uint64_t*)swapper_pg_dir;
        printk("swapper = %llx, src = %llx \n",swapper_pg_dir,src);
        uint64_t *dst = (uint64_t*)task[i]->pgd;
        for (int j = 0; j < PGSIZE / sizeof(uint64_t); j++) {
            dst[j] = src[j];
        }

    // #ifndef ELF
    //     // 为uapp分配专用内存并拷贝
    //     uint64_t uapp_size = (uint64_t)_eramdisk - (uint64_t)_sramdisk;
    //     uint64_t page_num = (uapp_size + PGSIZE - 1) / PGSIZE;  // 向上取整
    //     // 分配专用物理页
    //     char *uapp_copy = (char*)alloc_pages(page_num);
        
    //     // 手动拷贝uapp（从 _sramdisk 到 _eramdisk）
    //     char *uapp_src = (char*)_sramdisk;
    //     char *uapp_dst = (char*)uapp_copy;
    //     for (uint64_t j = 0; j < uapp_size; j++) {
    //         uapp_dst[j] = uapp_src[j];
    //     }
    //     uint64_t uapp_va = USER_START;  // 用户程序虚拟地址起始
    //     uint64_t uapp_pa = (uint64_t)uapp_copy-PA2VA_OFFSET;  // 分配的物理地址
    //     // printk("uapp_copy virtual: %p\n", uapp_copy);
    //     // printk("uapp_pa: 0x%lx\n", uapp_pa);
    //     for(uint64_t j = 0; j < page_num; j++) {
    //         create_mapping(task[i]->pgd, uapp_va + j * PGSIZE, uapp_pa + j * PGSIZE, PGSIZE,0x1f);
    //     }

    // #else
    //     load_program(task[i]);

    // #endif

    //     // 为用户态栈分配内存并映射
    //     char *user_stack = (char*)alloc_pages(1);  // 分配一个页面作为用户态栈
    //     uint64_t user_stack_va = USER_END - PGSIZE;  // 用户栈从USER_END向下生长
    //     uint64_t user_stack_pa = (uint64_t)(user_stack-PA2VA_OFFSET);
    //     create_mapping(
    //         task[i]->pgd,
    //         user_stack_va,
    //         user_stack_pa,
    //         PGSIZE,
    //         0x1f
    //     );
    //     //  设置用户态栈指针
    //     task[i]->thread.sscratch = user_stack_va + PGSIZE;  // 指向用户栈顶
    
        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)_sramdisk;
        task[i]->thread.sepc=ehdr->e_entry;
        Elf64_Phdr* phdrs = (Elf64_Phdr*)(_sramdisk + ehdr->e_phoff);
        
        for(int j = 0; j < ehdr->e_phnum; j++){
            Elf64_Phdr* phdr = &phdrs[j];
            
            if(phdr->p_type == PT_LOAD){
                // 计算VMA的flags
                uint64_t flags = 0;
                if(phdr->p_flags & PF_R) flags |= VM_READ;
                if(phdr->p_flags & PF_W) flags |= VM_WRITE; 
                if(phdr->p_flags & PF_X) flags |= VM_EXEC;
                
                // 调用do_mmap建立VMA映射
                do_mmap(&task[i]->mm, 
                        phdr->p_vaddr,        // 虚拟地址
                        phdr->p_memsz,        // 内存大小
                        phdr->p_offset,       // 文件偏移
                        phdr->p_filesz,       // 文件大小
                        flags);               // VMA权限标志
            }
        }
        do_mmap(&task[i]->mm, 
            USER_END - PGSIZE,    // 栈虚拟地址
            PGSIZE,               // 栈大小
            0,                    // 无文件映射
            0,                    // 无文件内容
            VM_ANON |VM_READ | VM_WRITE);  // 栈权限：可读可写

        task[i]->files=file_init();
    }
    printk("...task_init done!\n");
}


#if TEST_SCHED
#define MAX_OUTPUT ((NR_TASKS - 1) * 10)
char tasks_output[MAX_OUTPUT];
int tasks_output_index = 0;
char expected_output[] = "2222222222111111133334222222222211111113";
#include "sbi.h"
#endif

void dummy() {
    uint64_t MOD = 1000000007;
    uint64_t auto_inc_local_var = 0;
    int last_counter = -1;
    while (1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if (current->counter == 1) {
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
            #if TEST_SCHED
            tasks_output[tasks_output_index++] = current->pid + '0';
            if (tasks_output_index == MAX_OUTPUT) {
                for (int i = 0; i < MAX_OUTPUT; ++i) {
                    if (tasks_output[i] != expected_output[i]) {
                        printk("\033[31mTest failed!\033[0m\n");
                        printk("\033[31m    Expected: %s\033[0m\n", expected_output);
                        printk("\033[31m    Got:      %s\033[0m\n", tasks_output);
                        sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_NONE);
                    }
                }
                printk("\033[32mTest passed!\033[0m\n");
                printk("\033[32m    Output: %s\033[0m\n", expected_output);
                sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_NONE);
            }
            #endif
        }
    }
}


extern void __switch_to(struct task_struct *prev, struct task_struct *next);

void switch_to(struct task_struct *next) {
    // printk("current = 0x%lld\n",(uint64_t)current);
    // printk("next = 0x%lld\n",(uint64_t)next);
    if(current!=next){
        // printk("call __switch_to\n");
        struct task_struct* prev=current;
        current=next;
        printk(BLUE"switch to [%d]\n"CLEAR,next->pid);
        __switch_to(prev, next);
    }
}

void do_timer(){
    // printk("current = 0x%lld\n",(uint64_t)current);
    // printk("current pid = 0x%lld\n",current->pid);
    // printk("current counter = 0x%lld\n",current->counter);
    // if(current->pid==0||current->counter<=0){
    if(current==idle||current->counter<=0){
        schedule();
    }else{
        current->counter--;
        if(current->counter)return;
        schedule();
    }
}

void schedule(){
    int all_equals_to_zero=1;
    for (int i=1; i<=nr_tasks; i++) {
        if(task[i]->counter!=0){
            all_equals_to_zero=0;
            break;
        }
    }
    // printk("all equals to zero : %d\n",all_equals_to_zero);
    if(all_equals_to_zero){
        for(int i=1;i<=nr_tasks;i++){
            task[i]->counter=task[i]->priority;
        }
    }
    int max_counter_thread_index=1;
    for(int i=1;i<=nr_tasks;i++){
        // printk("task[%d]->counter = %lld, task[%d]->counter = %lld\n",i,task[i]->counter,max_counter_thread_index,task[max_counter_thread_index]->counter);
        if(task[i]->counter>task[max_counter_thread_index]->counter){
            max_counter_thread_index=i;
        }
    }
    // printk("max_counter_thread_index = %d\n",max_counter_thread_index);
    // printk("task[max_counter_thread_index] = %lld\n",task[max_counter_thread_index]);
    switch_to(task[max_counter_thread_index]);
}

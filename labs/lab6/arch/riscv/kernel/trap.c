#include "mm.h"
#include "printk.h"
#include "proc.h"
#include "stdint.h"
#include "clock.h"
#include "defs.h"
#include "syscall.h"
#include "vma.h"
#include "string.h"

extern struct task_struct *current;
extern void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);
extern char _sramdisk[];
void do_page_fault(struct pt_regs *regs);
void trap_handler(uint64_t scause, uint64_t sepc, struct pt_regs *regs) {
    // 提取异常码（去掉最高位）
    uint64_t exception_code = scause & 0x7FFFFFFFFFFFFFFF;
    
    if (scause & 0x8000000000000000) {
        // 中断处理
        if ((exception_code & 0x7) == 0x5) {
            // 时钟中断
            clock_set_next_event();
            do_timer();
        } else {
            printk("Unknown interrupt: scause=0x%lx\n", scause);
        }
    } else {
        // 异常处理
        switch (exception_code) {
            case 8:  // Environment call from U-mode
                regs->sepc += 4;  // 跳过 ecall 指令
                syscall(regs);    // 处理系统调用
                break;
            case 12:
            case 13:
            case 15:
                do_page_fault(regs);
                break;
            default:
                printk("Unknown exception: scause=0x%lx, sepc=0x%lx\n", scause, sepc);
                break;
            
        }
    }
}

void do_page_fault(struct pt_regs *regs) {
    uint64_t bad_addr = regs->stval;
    printk("got bad addr %lx\n",bad_addr);
    struct vm_area_struct *vma = find_vma(&current->mm, bad_addr);
    
    if (!vma) {
        printk("Page fault: no VMA for 0x%lx\n", bad_addr);
        return;
    }
    // 分配页面
    char *page = alloc_page();
    uint64_t page_addr = bad_addr & ~(PGSIZE - 1);
    
    if (vma->vm_flags & VM_ANON) {
        memset(page, 0, PGSIZE);
    } else {
        // 文件映射
        uint64_t file_offset = vma->vm_pgoff + (page_addr - vma->vm_start);
        uint64_t copy_size = (file_offset<vma->vm_filesz)?((file_offset +PGSIZE>= vma->vm_filesz) ? PGSIZE : vma->vm_filesz - file_offset):0;
        
        if (copy_size > 0) {
            char *file_src = (char*)_sramdisk + file_offset;
            for (int i = 0; i < copy_size; i++) {
                page[i] = file_src[i];
            }
        }
        if (copy_size < PGSIZE) {
            memset(page + copy_size, 0, PGSIZE - copy_size);
        }
    }

    // 建立映射
    create_mapping(current->pgd, page_addr, (uint64_t)page - PA2VA_OFFSET, PGSIZE, vma->vm_flags|0x11);
    asm volatile ("sfence.vma zero, zero");
}
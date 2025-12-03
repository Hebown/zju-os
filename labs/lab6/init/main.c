#include "defs.h"
#include "printk.h"
#include "proc.h"
#include <stdint.h>

extern void test();

extern uint64_t*_stext[],*_srodata[];
int start_kernel() {
    // uint64_t sscratch=csr_read(sscratch);
    // printk("pre sscratch = 0x%lx\n",sscratch);
    // csr_write(sscratch, 0x2);
    // sscratch=csr_read(sscratch);
    // printk("after sscratch = 0x%lx\n",sscratch);
    printk("\n2025\n");
    printk("\nZJU Operating System\n");
    // printk("run read test\n");
    // printk("_stext = %llx\n", *_stext);
    // printk("_srodata = %llx\n", *_srodata);
    // printk("read test finished\n");
    // printk("run write test\n");
    // *_stext = 0;
    // *_srodata = 0;
    // printk("write test finished\n");
    schedule();
    test();
    return 0;

}

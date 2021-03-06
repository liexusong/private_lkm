#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/stacktrace.h>
#include <linux/kallsyms.h>
#include <asm/page_32_types.h>
#include <linux/kprobes.h>

static int (*_kernel_text_address)(unsigned long) =
				(int (*)(unsigned long))0xc044bb89;

void print_addr(void *addr, int known)
{
	printk(KERN_ALERT " [%p] %s%pS\n", addr, known?"":" <?> ", addr);
}

static inline int in_stack(void *start, void *point, unsigned long size)
{
	unsigned long s, p;
	s = (unsigned long)start;
	p = (unsigned long)point;	
	return (s < p && p < s + THREAD_SIZE - size);
}

struct stackframe {
	struct stackframe *prev;
	void *ret_eip;		
} ;

static inline unsigned long get_bp(void)
{
	unsigned long bp;
	asm volatile ("movl %%ebp, %0" : "=r"(bp) :);
	return bp;
}

/*
 * can only work for current task
 * donot work for prev task
 */
void dump_stack_only_task(void)
{
	unsigned long stack, start, *p, bp, addr;
	struct stackframe *frame;
	
	bp = get_bp();
	frame = (struct stackframe *)bp;
	start = ((unsigned long)&stack & 0xfffff000);
	p = &stack;

	printk(KERN_ALERT "-------------[call track]-----------------------\n");
	printk(KERN_ALERT " [  addr  ] funcname+ret_addr_off/size [modname]\n");
	printk(KERN_ALERT "------------------------------------------------\n");
	while (in_stack((void *)start, p, sizeof(*p))) {
		addr = *p;
		if (_kernel_text_address(addr)) {
			if ((unsigned long)p == bp + sizeof(bp)) {	
				print_addr((void *)addr, 1);
				frame = frame->prev;
				bp = (unsigned long)frame;
			}
			else {
				print_addr((void *)addr, 0);
			}
		}
		p++;
	}
	printk(KERN_ALERT "-------------[end  track]-----------------------\n");
}

static int hook(unsigned long start, size_t len, long prot)
{
	dump_stack_only_task();
	jprobe_return();
	/*
	 * not run here, return from jprobe_return();
	 */
	return 0;
}

static struct jprobe pread_jp = {
	.entry = (kprobe_opcode_t *)hook,
};

int __init hello_init(void)
{
/*	char sym[KSYM_SYMBOL_LEN];*/
/*	sprint_symbol(sym, (unsigned long)a);
	printk(KERN_ALERT "{%s}", sym);
	printk(KERN_ALERT "{%p} hello kernel [%pS]\n", (void *)a, (void *)a);
*/
	int ret;
	pread_jp.kp.addr = 
			(kprobe_opcode_t *)kallsyms_lookup_name("vfs_getattr");
	if ((ret = register_jprobe(&pread_jp)) < 0) {
		printk(KERN_ALERT "jprobe register err\n");
		return -1;
	}
	printk(KERN_ALERT "init ok\n");
	return 0;
}

void __exit hello_exit(void)
{
	unregister_jprobe(&pread_jp);
	printk(KERN_ALERT "Goodbye kernel\n");
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("chobit_s <wangxiaochen0@gmail.com>");

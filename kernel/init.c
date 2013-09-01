#include <types.h>
#include <kmsg.h>
#include <kmalloc.h>
#include <ihk/cpu.h>
#include <ihk/mm.h>
#include <ihk/debug.h>
#include <ihk/dma.h>
#include <ihk/perfctr.h>
#include <process.h>
#include <init.h>
#include <cls.h>

//#define IOCTL_FUNC_EXTENSION
#ifdef IOCTL_FUNC_EXTENSION
#include <ioctl.h>
#endif

//#define DEBUG_PRINT_INIT

#ifdef DEBUG_PRINT_INIT
#define dkprintf kprintf
#else
#define dkprintf(...)
#endif


extern struct ihk_kmsg_buf kmsg_buf;

extern long syscall(int, ihk_mc_user_context_t *);

static void handler_init(void)
{
	ihk_mc_set_syscall_handler(syscall);
}

unsigned long data[1024] __attribute__((aligned(64)));

static void dma_test(void)
{
	struct ihk_dma_request req;
	unsigned long fin = 0;
	int i, j;

	for (j = 0; j < 2; ++j) {
		fin = 0;

		memset(data, 0, 1024 * sizeof(unsigned long));

		for (i = 0; i < 8; i++) {
			data[i] = i;
		}

		kprintf("DMA Test Started.\n");
		memset(&req, 0, sizeof(req));
		req.src_os = IHK_THIS_OS;
		req.src_phys = virt_to_phys(data);
		req.dest_os = IHK_THIS_OS;
		req.dest_phys = virt_to_phys(&data[256]);
		req.size = 64;
		req.notify = (void *)virt_to_phys(&fin);
		req.notify_os = IHK_THIS_OS;
		req.priv = (void *)0x2984;

		kprintf("VtoP : %p, %lx\n", data, virt_to_phys(data));
		kprintf("notify : %p, %lx (%lx)\n", &fin, virt_to_phys(&fin),
				sizeof(req));

		if (ihk_mc_dma_request(0, &req) != 0) {
			kprintf("Failed to request DMA!\n");
		}
		kprintf("DMA Test Wait.\n");
		while (!fin) {
			barrier();
		}
		kprintf("DMA Test End.\n");

		for (i = 0; i < 8; i++) {
			if (data[i] != data[256 + i]) {
				kprintf("DMA result is inconsistent!\n");
				panic("");
			}
		}
	}
}

extern char *ihk_mc_get_kernel_args(void);

char *find_command_line(char *name)
{
	char *cmdline = ihk_mc_get_kernel_args();

	if (!cmdline) {
		return NULL;
	}
	return strstr(cmdline, name);
}

void pc_init(void)
{
	int i;
	int kmode = PERFCTR_KERNEL_MODE;
	int imode = 1;
	char *p;

	int x[2][4] = { { APT_TYPE_INSTRUCTIONS_EXECUTED,
	                  APT_TYPE_DATA_READ_MISS,
	                  APT_TYPE_L2_CODE_READ_MISS_MEM_FILL,
                      APT_TYPE_CODE_CACHE_MISS, },
	                { APT_TYPE_CODE_CACHE_MISS,
                      APT_TYPE_LLC_MISS, // not updated for KNC
	                  APT_TYPE_STALL, APT_TYPE_CYCLE }, // not updated for KNC
	};

	if (!(p = find_command_line("perfctr"))) {
		dkprintf("perfctr not initialized.\n");
		return;
	}
	if (p[7] == '=' && p[8] >= '0' && p[8] <= '5') {
		i = p[8] - '0';
		kmode = (i >> 1) + 1;
		imode = (i & 1);
	} else {
		dkprintf("perfctr not initialized.\n");
		return;
	}
	dkprintf("perfctr mode : priv = %d, set = %d\n", kmode, imode);

	for (i = 0; i < 4; i++) {
		ihk_mc_perfctr_init(i, x[imode][i], kmode);
	}
	ihk_mc_perfctr_start(0xf);
}

void pc_ap_init(void)
{
	pc_init();
}

static void pc_test(void)
{
	int i;
	unsigned long st[4], ed[4];

	pc_init();

	ihk_mc_perfctr_read_mask(0xf, st);
	for (i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
		data[i] += i;
		asm volatile ("" : : : "memory");
	}
	ihk_mc_perfctr_read_mask(0xf, ed);

	kprintf("perfctr:(%ld) %ld, %ld, %ld, %ld\n", st[0], ed[0] - st[0],
	        ed[1] - st[1], ed[2] - st[2], ed[3] - st[3]);
}

static void rest_init(void)
{
	char *cmdline;
	cmdline = ihk_mc_get_kernel_args();
	kprintf("KCommand Line: %s\n", cmdline);

	handler_init();

	ihk_mc_dma_init();
	dma_test();
	//pc_test();

	ap_init();
	cpu_local_var_init();
	kmalloc_init();

	ikc_master_init();

	sched_init();
}

int host_ikc_inited = 0;

static void post_init(void)
{
	cpu_enable_interrupt();

	while (!host_ikc_inited) {
		barrier();
		cpu_pause();
	}

	if (find_command_line("hidos")) {
		init_host_syscall_channel();
		init_host_syscall_channel2();
	}
	ap_start();
}
#ifdef DCFA_RUN
extern void user_main();
#endif

#ifdef DCFA_KMOD
extern int mc_cmd_client_init(void);
extern void ibmic_cmd_init(void);
#endif

int main(void)
{
	kmsg_init();

	kputs("MCK started.\n");

	arch_init();

	mem_init();

	rest_init();

	arch_ready();

	post_init();

	futex_init();

	kputs("MCK/IHK booted.\n");

#ifdef DCFA_KMOD
	mc_cmd_client_init();
#ifdef CMD_DCFA
	ibmic_cmd_init();
#endif
#endif

#ifdef DCFA_RUN
	kputs("DCFA begin\n");

	user_main();

	kputs("DCFA end\n");
#else
	schedule();
#endif

	return 0;
}

/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__   //为了包含定义在unistd.h中的系统调用号和内嵌汇编代码_syscall0()等
#include <unistd.h>
#include <time.h>

/*
 *inline关键字修饰的函数为内嵌函数
 *作用:直接将代码内嵌进来，不会发生调用，不会使用堆栈
 *fork()、pause()需要使用内嵌方式，以保证main()不会弄乱堆栈
 */
static inline _syscall0(int,fork)
static inline _syscall0(int,pause)
	/*
	 * we need this inline内嵌语句 - forking from kernel space will result
	 * in NO COPY ON WRITE (没有写时复制!!!), until an execve is executed. This
	 * is no problem, but for the stack. This is handled by not letting
	 * main() use the stack at all after fork(). Thus, no function
	 * calls - which means inline code for fork too, as otherwise we
	 * would use the stack upon exit from 'fork()'.
	 *
	 * Actually only pause and fork are needed inline, so that there
	 * won't be any messing with the stack from main(), but we define
	 * some others too.
	 */


static inline _syscall1(int,setup,void *,BIOS)   //仅在此程序的init()被调用
static inline _syscall0(int,sync)

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/fs.h>

static char printbuf[1024];

extern int vsprintf();
extern void init(void);
extern void blk_dev_init(void);
extern void chr_dev_init(void);
extern void hd_init(void);
extern void floppy_init(void);
extern void mem_init(long start, long end);
extern long rd_init(long mem_start, int length);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;

/*
 * This is set up by the setup-routine at boot-time
 */
#define EXT_MEM_K (*(unsigned short *)0x90002)      //1MB以后的扩展内存大小（KB）
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;
	startup_time = kernel_mktime(&time);
}






static long memory_end = 0;            //机器具有的物理内存容量(字节数)
static long buffer_memory_end = 0;     //高速缓冲区end地址
static long main_memory_start = 0;     //主内存开始位置

struct drive_info { char dummy[32]; } drive_info; //用于存放硬盘参数表info

void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (在head.s程序136行的那几行代码) this */


/*此时中断仍被禁止着. Do necessary setups, 再打开中断*/


/*1.对物理内存各部分进行功能划分与分配*/
 	ROOT_DEV = ORIG_ROOT_DEV;     //根设备号（放在0x901FC）
 	drive_info = DRIVE_INFO;      //硬盘参数表信息(放在0x90080)
	/*主内存数memory_end*/
	memory_end = (1<<20) + (EXT_MEM_K<<10);  //内存大小=1Mb+扩展内存(kB)*1024字节
	memory_end &= 0xfffff000;                //最后不到4kb（1页）的内存碎片忽略掉
	if (memory_end > 16*1024*1024)           //如果内存超过16Mb，则按16Mb计
		memory_end = 16*1024*1024;

	/*高速缓冲区末端buffer_memory_end*/
	if (memory_end > 12*1024*1024)           //根据主内存大小, 设置缓冲区大小
		buffer_memory_end = 4*1024*1024;     
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
	
	/*主内存开始地址main_memory_start*/
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK    //如果makefile定义了RAMDISK，则初始化RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024); //main_memory_start加上RAMDISK大小
#endif

	mem_init(main_memory_start,memory_end);

/*2.对系统各部分初始化*/
	trap_init();        //kernel/traps.c
	
	blk_dev_init();     //kernel/blk_drv/ll_rw_blk.c
	hd_init();          //kernel/blk_drv/hd.c
	floppy_init();      //kernel/blk_drv/floppy.c
	
	chr_dev_init();     //kernel/chr_drv/tty_io.c
	tty_init();         //kernel/chr_drv/tty_io.c
	
	time_init();
	
	sched_init();       //kernel/sched.c
	
	buffer_init(buffer_memory_end); //fs/buffer.c

/*所有初始化工作done，开启中断*/
	sti();

/*移动到用户模式下，进程0执行*/
	move_to_user_mode();

	if (!fork()) {	   //父进程执行fork()，成功创建后返回0给子进程
		init();              //子进程执行init()
	}
	for(;;) pause();   //父进程空闲时执行pause()
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
}






static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL };

static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

void init(void)
{
	int pid,i;

    /*为系统调用sys_setup(),在上面25行的宏定义_syscall1(int,setup,void *,BIOS)*/
	setup((void *) &drive_info);   
	
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);
		execve("/bin/sh",argv_rc,envp_rc);
		_exit(2);
	}
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;
	while (1) {
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			close(0);close(1);close(2);
			setsid();
			(void) open("/dev/tty0",O_RDWR,0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh",argv,envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}

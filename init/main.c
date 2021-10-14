/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__   //Ϊ�˰���������unistd.h�е�ϵͳ���úź���Ƕ������_syscall0()��
#include <unistd.h>
#include <time.h>

/*
 *inline�ؼ������εĺ���Ϊ��Ƕ����
 *����:ֱ�ӽ�������Ƕ���������ᷢ�����ã�����ʹ�ö�ջ
 *fork()��pause()��Ҫʹ����Ƕ��ʽ���Ա�֤main()����Ū�Ҷ�ջ
 */
static inline _syscall0(int,fork)
static inline _syscall0(int,pause)
	/*
	 * we need this inline��Ƕ��� - forking from kernel space will result
	 * in NO COPY ON WRITE (û��дʱ����!!!), until an execve is executed. This
	 * is no problem, but for the stack. This is handled by not letting
	 * main() use the stack at all after fork(). Thus, no function
	 * calls - which means inline code for fork too, as otherwise we
	 * would use the stack upon exit from 'fork()'.
	 *
	 * Actually only pause and fork are needed inline, so that there
	 * won't be any messing with the stack from main(), but we define
	 * some others too.
	 */


static inline _syscall1(int,setup,void *,BIOS)   //���ڴ˳����init()������
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
#define EXT_MEM_K (*(unsigned short *)0x90002)      //1MB�Ժ����չ�ڴ��С��KB��
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






static long memory_end = 0;            //�������е������ڴ�����(�ֽ���)
static long buffer_memory_end = 0;     //���ٻ�����end��ַ
static long main_memory_start = 0;     //���ڴ濪ʼλ��

struct drive_info { char dummy[32]; } drive_info; //���ڴ��Ӳ�̲�����info

void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (��head.s����136�е��Ǽ��д���) this */


/*��ʱ�ж��Ա���ֹ��. Do necessary setups, �ٴ��ж�*/


/*1.�������ڴ�����ֽ��й��ܻ��������*/
 	ROOT_DEV = ORIG_ROOT_DEV;     //���豸�ţ�����0x901FC��
 	drive_info = DRIVE_INFO;      //Ӳ�̲�������Ϣ(����0x90080)
	/*���ڴ���memory_end*/
	memory_end = (1<<20) + (EXT_MEM_K<<10);  //�ڴ��С=1Mb+��չ�ڴ�(kB)*1024�ֽ�
	memory_end &= 0xfffff000;                //��󲻵�4kb��1ҳ�����ڴ���Ƭ���Ե�
	if (memory_end > 16*1024*1024)           //����ڴ泬��16Mb����16Mb��
		memory_end = 16*1024*1024;

	/*���ٻ�����ĩ��buffer_memory_end*/
	if (memory_end > 12*1024*1024)           //�������ڴ��С, ���û�������С
		buffer_memory_end = 4*1024*1024;     
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
	
	/*���ڴ濪ʼ��ַmain_memory_start*/
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK    //���makefile������RAMDISK�����ʼ��RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024); //main_memory_start����RAMDISK��С
#endif

	mem_init(main_memory_start,memory_end);

/*2.��ϵͳ�����ֳ�ʼ��*/
	trap_init();        //kernel/traps.c
	
	blk_dev_init();     //kernel/blk_drv/ll_rw_blk.c
	hd_init();          //kernel/blk_drv/hd.c
	floppy_init();      //kernel/blk_drv/floppy.c
	
	chr_dev_init();     //kernel/chr_drv/tty_io.c
	tty_init();         //kernel/chr_drv/tty_io.c
	
	time_init();
	
	sched_init();       //kernel/sched.c
	
	buffer_init(buffer_memory_end); //fs/buffer.c

/*���г�ʼ������done�������ж�*/
	sti();

/*�ƶ����û�ģʽ�£�����0ִ��*/
	move_to_user_mode();

	if (!fork()) {	   //������ִ��fork()���ɹ������󷵻�0���ӽ���
		init();              //�ӽ���ִ��init()
	}
	for(;;) pause();   //�����̿���ʱִ��pause()
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

    /*Ϊϵͳ����sys_setup(),������25�еĺ궨��_syscall1(int,setup,void *,BIOS)*/
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

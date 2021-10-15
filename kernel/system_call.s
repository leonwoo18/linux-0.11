/*
 *  linux/kernel/system_call.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  system_call.s  ����  system_call��int 0x80�жϣ� low-level handling routines.
 * This also contains the ʱ���ж� handler, as some of the code is
 * the same. The Ӳ�̺������ж� are also here.
 *
 * NOTE: This code handles �ź�ʶ��, which happens every time after a ʱ���ж�(int 0x20) and ϵͳ����(int 0x80). 
 *��ͨ�ж� ���ô��� �ź�ʶ��, ������ϵͳ��ɻ���.
 *
 */

SIG_CHLD	= 17     #�ӽ���ֹͣ�����

EAX		= 0x00        #�����Ĵ����ڶ�ջ�е�ƫ��λ��
EBX		= 0x04
ECX		= 0x08
EDX		= 0x0C
FS		= 0x10
ES		= 0x14
DS		= 0x18
EIP		= 0x1C
CS		= 0x20
EFLAGS		= 0x24
OLDESP		= 0x28
OLDSS		= 0x2C

state	= 0	     	  # task-struct �ṹ��ĳ�Ա������ƫ��ֵ
counter	= 4
priority = 8
signal	= 12
sigaction = 16	 
blocked = (33*16)


sa_handler = 0        # sigaction�ṹ��ĳ�Ա������ƫ��ֵ
sa_mask = 4
sa_flags = 8
sa_restorer = 12

nr_system_calls = 72     #linux 0.11���ں��е�ϵͳ��������




.globl system_call,sys_fork,timer_interrupt,sys_execve
.globl hd_interrupt,floppy_interrupt,parallel_interrupt
.globl device_not_available, coprocessor_error




.align 2
bad_sys_call:
	movl $-1,%eax
	iret

.align 2
reschedule:
	pushl $ret_from_sys_call    #ѹջ�����ִ�У�Ϊ�����
	jmp schedule                #����schedule.c��ִ���м�3��
		
.align 2
system_call:     #����۵�һ����ڣ�int 0x80���
	cmpl $nr_system_calls-1,%eax
	ja bad_sys_call
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx		# push %ebx,%ecx,%edx as parameters
	pushl %ebx		# to the system call
	movl $0x10,%edx		# set up ds,es to kernel space
	mov %dx,%ds
	mov %dx,%es
	movl $0x17,%edx		# fs points to local data space
	mov %dx,%fs
	call sys_call_table(,%eax,4)   #���eax�Ĵ����еĹ��ܺ�__NR_xxx������sys_xxx()
	pushl %eax
	movl current,%eax
	cmpl $0,state(%eax)		# state
	jne reschedule
	cmpl $0,counter(%eax)		# counter
	je reschedule            #���������reschedule���
	
/*
 * ��ջ���� in 'ret_from_system_call':
 *
 *	 0(%esp) - %eax
 *	 4(%esp) - %ebx
 *	 8(%esp) - %ecx
 *	 C(%esp) - %edx
 *	10(%esp) - %fs
 *	14(%esp) - %es
 *	18(%esp) - %ds
 *	1C(%esp) - %eip
 *	20(%esp) - %cs
 *	24(%esp) - %eflags
 *	28(%esp) - %oldesp
 *	2C(%esp) - %oldss
 */
ret_from_sys_call:         #��������һ�����
	movl current,%eax		# task[0] cannot have signals
	cmpl task,%eax
	je 3f
	cmpw $0x0f,CS(%esp)		# was old code segment supervisor ?
	jne 3f
	cmpw $0x17,OLDSS(%esp)		# was stack segment = 0x17 ?
	jne 3f
	movl signal(%eax),%ebx
	movl blocked(%eax),%ecx
	notl %ecx
	andl %ebx,%ecx
	bsfl %ecx,%ecx
	je 3f
	btrl %ecx,%ebx
	movl %ebx,signal(%eax)
	incl %ecx
	pushl %ecx
	call do_signal        #����do_signal()
	popl %eax
3:	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	pop %fs
	pop %es
	pop %ds
	iret



#sys_fork()��sys_execve()�ǱȽ����������ϵͳ����
#��Ҫ�ȴӻ�������ڣ����ҵ������ʵ�ֺ���
.align 2
sys_execve:                  
	lea EIP(%esp),%eax
	pushl %eax
	call do_execve            #sys_execve---->do_execve()
	addl $4,%esp
	ret

.align 2
sys_fork:
	call find_empty_process   #sys_fork---->find_empty_process()
	testl %eax,%eax
	js 1f
	push %gs
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	call copy_process         #sys_fork--->copy_process()
	addl $20,%esp
1:	ret




#����ʱ���жϡ�Ӳ�̡������жϴ����������system_call�Ƚ�����
#���Զ�Ӧ���жϴ������Ҳ������

.align 2
coprocessor_error:          #int 0x16 ��Э����������
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	pushl $ret_from_sys_call
	jmp math_error

.align 2
device_not_available:       #int 0x7���豸������
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	pushl $ret_from_sys_call
	clts				# clear TS so that we can use math
	movl %cr0,%eax
	testl $0x4,%eax			# EM (math emulation bit)
	je math_state_restore
	pushl %ebp
	pushl %esi
	pushl %edi
	call math_emulate
	popl %edi
	popl %esi
	popl %ebp
	ret

.align 2
timer_interrupt:         #int 0x20 ʱ���ж�
	push %ds		# save ds,es and put kernel data space
	push %es		# into them. %fs is used by _system_call
	push %fs
	pushl %edx		# we save %eax,%ecx,%edx as gcc doesn't
	pushl %ecx		# save those across function calls. %ebx
	pushl %ebx		# is saved as we use that in ret_sys_call
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	incl jiffies
	movb $0x20,%al		# EOI to interrupt controller #1
	outb %al,$0x20
	movl CS(%esp),%eax
	andl $3,%eax		# %eax is CPL (0 or 3, 0=supervisor)
	pushl %eax
	call do_timer		# 'do_timer(long CPL)' does everything from
	addl $4,%esp		# task switching to accounting ...
	jmp ret_from_sys_call


hd_interrupt:                 #int 0x46�������жϴ������
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0xA0		# EOI to interrupt controller #1
	jmp 1f			# give port chance to breathe
1:	jmp 1f
1:	xorl %edx,%edx
	xchgl do_hd,%edx
	testl %edx,%edx
	jne 1f
	movl $unexpected_hd_interrupt,%edx
1:	outb %al,$0x20
	call *%edx		# "interesting" way of handling intr.
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

/*
 * Ok, I get ���д�ӡ���ж� while using the ���� for some strange reason. 
 *Urgel���ߣ�. Now I just ignore them.
 */

floppy_interrupt:            #int 0x48�������������жϴ������
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0x20		# EOI to interrupt controller #1
	xorl %eax,%eax
	xchgl do_floppy,%eax
	testl %eax,%eax
	jne 1f
	movl $unexpected_floppy_interrupt,%eax
1:	call *%eax		# "interesting" way of handling intr.
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

parallel_interrupt:          #int 0x39 (int 0x27):���п��жϴ������
	pushl %eax
	movb $0x20,%al
	outb %al,$0x20
	popl %eax
	iret

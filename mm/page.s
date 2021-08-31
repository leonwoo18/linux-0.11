/*
 *  linux/mm/page.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * page.s contains the low-level page-exception code.
 * the real work is done in mm.c
 */

.globl page_fault

page_fault:
	xchgl %eax,(%esp)   /*取出错码error_code到eax*/
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%edx
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	movl %cr2,%edx      /*从CR2中取页出错时的虚拟address到edx*/
	
	pushl %edx          /*将出错码error_code和虚拟address压入栈中*/
	pushl %eax          /*作为do_no_page()或do_wp_page()的参数*/
	
	testl $1,%eax
	jne 1f
	call do_no_page  /*缺页(no)引起的页异常*/
	jmp 2f
1:	call do_wp_page  /*写保护(write protect)引起的页异常*/
2:	addl $8,%esp
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

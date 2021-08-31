#
# if you want the ram-disk device, define this to be the
# size in blocks.
#
RAMDISK =  #-DRAMDISK=512

#8086编译器和连接器
AS86	=as86 -0 -a
LD86	=ld86 -0
#GNU编译器和连接器
AS	=as
LD	=ld

#指定编译器为gcc-3.4；编译出的指令为80386cpu的指令
CC	=gcc-3.4 -march=i386 $(RAMDISK)  

#gcc编译器选项  ：-Wall=warn all , -o=optimze 
CFLAGS	=-m32 -g -Wall -O2 -fomit-frame-pointer 
#ld连接器选项   ：elf_i386为链接工具, startup_32为内存位置0x100000
LDFLAGS	=-m elf_i386 -Ttext 0 -e startup_32


#cpp：gcc的预处理程序
#-nostdinc: 不要去std标准目录中的inc头文件
#-Iinclude: -I指定目录
CPP	=cpp -nostdinc -Iinclude





#
# ROOT_DEV specifies the default root-device when making the image.
# This can be either FLOPPY, /dev/xxxx or empty, in which case the
# default of /dev/hd6 is used by 'build'.

# build工具用到的以$(ROOT_DEV)为根文件系统设备组装成内核映像Image
ROOT_DEV= #FLOPPY 

#==============依赖在下方=================
ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o
DRIVERS =kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a
MATH	=kernel/math/math.a
LIBS	=lib/lib.a
#『====ARCHIVES、DRIVERS、MATH、LIBS的依赖=========
kernel/math/math.a: FORCE
	(cd kernel/math; make)

kernel/blk_drv/blk_drv.a: FORCE
	(cd kernel/blk_drv; make)

kernel/chr_drv/chr_drv.a: FORCE
	(cd kernel/chr_drv; make)

kernel/kernel.o: FORCE
	(cd kernel; make)

mm/mm.o: FORCE
	(cd mm; make)

fs/fs.o: FORCE
	(cd fs; make)

lib/lib.a: FORCE
	(cd lib; make)
#====ARCHIVES、DRIVERS、MATH、LIBS的依赖=========』




.c.s:
	$(CC) $(CFLAGS) \
	                   #-S:只翻译成.s汇编语言
	-nostdinc -Iinclude -S -o $*.s $<
.s.o:
	$(AS)  -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) \
	                   #-C:只编译成目标文件，不链接
	-nostdinc -Iinclude -c -o $*.o $<







#make/makeall 入口
all:	Image

Image: boot/bootsect boot/setup tools/system tools/build
    #将system模块复制为system.tmp，并strip减少文件大小
	#并用objcopy工具(-R .note -R .comment删减掉注释和评论)，转换为kernel可执行二进制文件
	cp -f tools/system system.tmp
	strip system.tmp
	objcopy -O binary -R .note -R .comment system.tmp tools/kernel
#核心的一句：用build工具，把bootsect、setup、system（这里已copy为kernel）合成一个镜像文件Image
	tools/build boot/bootsect boot/setup tools/kernel $(ROOT_DEV) > Image
	rm system.tmp
	rm tools/kernel -f
	sync

disk: Image
    #dd=disk dump,bs=bytes; if=infile; of=outfile
	dd bs=8192 if=Image of=/dev/fd0

BootImage: boot/bootsect boot/setup tools/build
	tools/build boot/bootsect boot/setup none $(ROOT_DEV) > Image
	sync

tools/build: tools/build.c
	gcc $(CFLAGS) \
	-o tools/build tools/build.c

boot/head.o: boot/head.s
	gcc-3.4 -m32 -g -I./include -traditional -c boot/head.s
	mv head.o boot/

tools/system:	boot/head.o init/main.o \
		$(ARCHIVES) $(DRIVERS) $(MATH) $(LIBS)
	#将所有东西链接为system模块
	$(LD) $(LDFLAGS) boot/head.o init/main.o \
	$(ARCHIVES) \
	$(DRIVERS) \
	$(MATH) \
	$(LIBS) \
	-o tools/system 
	#> System.map表示ld需要将连接映像重定向保存在System.map文件中
	nm tools/system | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aU] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)'| sort > System.map 
	
#setup.s和bootsect.s使用的是8086汇编和链接器
boot/setup: boot/setup.s
	$(AS86) -o boot/setup.o boot/setup.s
	$(LD86) -s -o boot/setup boot/setup.o
boot/bootsect:	boot/bootsect.s
	$(AS86) -o boot/bootsect.o boot/bootsect.s
	$(LD86) -s -o boot/bootsect boot/bootsect.o

#往bootsect.s程序开头添加一行system文件长度的信息
tmp.s:	boot/bootsect.s tools/system
	(echo -n "SYSSIZE = (";ls -l tools/system | grep system \
		| cut -c25-31 | tr '\012' ' '; echo "+ 15 ) / 16") > tmp.s
	cat boot/bootsect.s >> tmp.s
	



#make clean入口
clean:
	rm -f Image System.map tmp_make core boot/bootsect boot/setup
	rm -f init/*.o tools/system tools/build boot/*.o
	(cd mm;make clean)
	(cd fs;make clean)
	(cd kernel;make clean)
	(cd lib;make clean)


#先执行上面的clean，再对/linux目录进行压缩，最终生成backup.Z压缩文件
backup: clean
	(cd .. ; tar cf - linux | compress16 - > backup.Z)
	sync







dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in init/*.c;do echo -n "init/";$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile
	(cd fs; make dep)
	(cd kernel; make dep)
	(cd mm; make dep)

# Force make run into subdirectories even no changes on source
FORCE:

### Dependencies:
init/main.o: init/main.c include/unistd.h include/sys/stat.h \
  include/sys/types.h include/sys/times.h include/sys/utsname.h \
  include/utime.h include/time.h include/linux/tty.h include/termios.h \
  include/linux/sched.h include/linux/head.h include/linux/fs.h \
  include/linux/mm.h include/signal.h include/asm/system.h \
  include/asm/io.h include/stddef.h include/stdarg.h include/fcntl.h

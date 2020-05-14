Implement new System Calls to use Kprobe for dynamic dump stack

Hardware: Intel Galileo Gen2 

Software: gcc: i586 poky linux

Linux Version: 3.19.18

Content:
- syscall.patch: Kernel Patch created between original kernel source and source modified after syscall impelentation changes.
- main.c: Test program to insert, remove, clean all kprobe symbols after exit.
- Makefile: makefile to compile the test program. The makefile create executable called dump_stack_tester

Introduction:
                                        insdump and rmdump system call  
- Exposes System call interface to insert(insdump) and remove dump(rmdump)
- SYSCALL_DEFINE2(insdump, const char __user *, symbolname, dumpmode_t, mode)
- SYSCALL_DEFINE1(rmdump, unsigned int, dumpid)
- insdump = 359 and rmdump = 360 at syscall_32.tbl
- Configured from menuconfig via CONFIG_DYNAMIC_DUMP_STACK
- insdump 
   - SYSCALL_DEFINE2(insdump, const char __user *,symbolname, dumpmode_t __user *,mode)
   - provides interface to register kprobes for valid symbols from /proc/kallsyms and static symbols from text segment mapped to System.map
   - returns unique positive non zero identifier to userspace on success or negative number if error
   - provides "MAX_PROBE_PER_PID" symbols per pid. Currently set to 10
   - only unique sysmbols are registered with kprobe per pid 
   - dump_stack() is called from kprobe prehandler and which process can use dump is controlled by mode
   - mode = 0, only process which registered kprobe can use dump
   - mode = 1, process whoregistered and all his siblings
   - mode > 1, any process and access the dump_stack
- rmdump
   - SYSCALL_DEFINE1(rmdump, unsigned int, dumpid)
   - provides user intreface to remove dump trace for particular symbol via unique dumpid 
   - dumpid 0 is reserved to remove all the dump_stack traces for particular pid 
   - dumpid o is useful when process is exiting or throughfully wants to remove all the traces
 

Steps:
- Apply the patch from the directory above kernel directory.
- run make menuconfig and select kernel configuration to enable trace dump. Kernel Hacking -> DYNAMIC DUMP STACK
- save config.
- build.sh  - the kernel. The build.sh inside kernel can be used to build the kernel as well.   
-           - builds main.c and copy  to /tftpboot.


Sample Output: Using two threads adding fur different symbols - sys_getcpu, sys_time, sys_stime, sys_umask. The threads not only adds the symbols but also executes the system calls.

Snapshot for Mode 0
root@quark:~/ass4# ./dump_stack_tester

Successfully added Symbol: sys_getcpu with 1 for pid = 296
Successfully added Symbol: sys_getcpu with 2 for pid = 295
Successfully added Symbol: sys_time with 3 for pid = 296
Successfully added Symbol: sys_time with 4 for pid = 295
Successfully added Symbol: sys_stime with 5 for pid = 296
Successfully added Symbol: sys_stime with 6 for pid = 295
Successfully added Symbol: sys_umask with 7 for pid = 296
Successfully added Symbol: sys_umask with 8 for pid = 295
Starting to test[   58.797191] CPU: 0 PID: 296 Comm: dump_stack_test Not tainted 3.19.8-yocto-standard #1
[   58.806428] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[   58.806912]  00000126 00000126 cd241f3c c1453ba1 cd241f48 c124cea9 cd7d2034 cd241f60
[   58.806912]  c10a2762 cd7d203c 00000246 4c089d40 08048af4 cd241f6c c102853a b6f7f2ec
[   58.806912]  cd240000 d286201b b6f7f2ec b6f7f2e8 000000e0 4c089d40 08048af4 cd240000
[   58.806912] Call Trace:
[   58.806912]  [<c1453ba1>] dump_stack+0x16/0x18
[   58.806912]  [<c124cea9>] dumpstack_handler_pre+0x49/0x60
[   58.806912]  [<c10a2762>] opt_pre_handler+0x32/0x60
[   58.806912]  [<c102853a>] optimized_callback+0x5a/0x70
[   58.806912]  [<c111007b>] ? unuse_mm+0x1db/0x420
[   58.806912]  [<c10472f1>] ? SyS_getcpu+0x1/0x50
[   58.806912]  [<c14575c4>] ? syscall_call+0x7/0x7
 pid 296
Starting to test[   58.876305] CPU: 0 PID: 295 Comm: dump_stack_test Not tainted 3.19.8-yocto-standard #1
[   58.884646] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[   58.884646]  00000126 00000126 ce665f3c c1453ba1 ce665f48 c124cea9 cd7d2834 ce665f60
[   58.884646]  c10a2762 cd7d283c 00000246 4c089d40 08048af4 ce665f6c c102853a b777f2ec
[   58.884646]  ce664000 d286201b b777f2ec b777f2e8 000000e0 4c089d40 08048af4 ce664000
[   58.884646] Call Trace:
[   58.884646]  [<c1453ba1>] dump_stack+0x16/0x18
[   58.884646]  [<c124cea9>] dumpstack_handler_pre+0x49/0x60
[   58.884646]  [<c10a2762>] opt_pre_handler+0x32/0x60
[   58.884646]  [<c102853a>] optimized_callback+0x5a/0x70
[   58.884646]  [<c111007b>] ? unuse_mm+0x1db/0x420
[   58.884646]  [<c10472f1>] ? SyS_getcpu+0x1/0x50
[   58.884646]  [<c14575c4>] ? syscall_call+0x7/0x7
 pid 295
[   58.953963] CPU: 0 PID: 296 Comm: dump_stack_test Not tainted 3.19.8-yocto-standard #1
[   58.962608] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[   58.963740]  00000126 00000126 cd241f3c c1453ba1 cd241f48 c124cea9 cd7d20a8 cd241f60
[   58.963740]  c10a2762 cd7d20b0 00000246 4c089d40 08048af4 cd241f6c c102853a b6f7f2e4
[   58.963740]  cd240000 d286205c b6f7f2e4 b6f7f2e8 000000e0 4c089d40 08048af4 cd240000
[   58.963740] Call Trace:
[   58.963740]  [<c1453ba1>] dump_stack+0x16/0x18
[   58.963740]  [<c124cea9>] dumpstack_handler_pre+0x49/0x60
[   58.963740]  [<c10a2762>] opt_pre_handler+0x32/0x60
[   58.963740]  [<c102853a>] optimized_callback+0x5a/0x70
[   58.963740]  [<c111007b>] ? unuse_mm+0x1db/0x420
[   58.963740]  [<c1073d71>] ? SyS_time+0x1/0x30
[   58.963740]  [<c14575c4>] ? syscall_call+0x7/0x7
[   59.050174] CPU: 0 PID: 295 Comm: dump_stack_test Not tainted 3.19.8-yocto-standard #1
[   59.058145] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[   59.060053]  00000126 00000126 ce665f3c c1453ba1 ce665f48 c124cea9 cd7d28a8 ce665f60
[   59.060053]  c10a2762 cd7d28b0 00000246 4c089d40 08048af4 ce665f6c c102853a b777f2e4
[   59.060053]  ce664000 d286205c b777f2e4 b777f2e8 000000e0 4c089d40 08048af4 ce664000
[   59.060053] Call Trace:
[   59.060053]  [<c1453ba1>] dump_stack+0x16/0x18
[   59.060053]  [<c124cea9>] dumpstack_handler_pre+0x49/0x60
[   59.060053]  [<c10a2762>] opt_pre_handler+0x32/0x60
[   59.060053]  [<c102853a>] optimized_callback+0x5a/0x70
[   59.060053]  [<c111007b>] ? unuse_mm+0x1db/0x420
[   59.060053]  [<c1073d71>] ? SyS_time+0x1/0x30
[   59.060053]  [<c14575c4>] ? syscall_call+0x7/0x7
[   59.126283] CPU: 0 PID: 296 Comm: dump_stack_test Not tainted 3.19.8-yocto-standard #1
[   59.134253] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[   59.136160]  00000126 00000126 cd241f3c c1453ba1 cd241f48 c124cea9 cd7d211c cd241f60
[   59.136160]  c10a2762 cd7d2124 00000246 4c089d40 594b2ecf cd241f6c c102853a b6f7f2e4
[   59.136160]  cd240000 d286209d b6f7f2e4 b6f7f2e8 000000e0 4c089d40 594b2ecf cd240000
[   59.136160] Call Trace:
[   59.136160]  [<c1453ba1>] dump_stack+0x16/0x18
[   59.136160]  [<c124cea9>] dumpstack_handler_pre+0x49/0x60
[   59.136160]  [<c10a2762>] opt_pre_handler+0x32/0x60
[   59.136160]  [<c102853a>] optimized_callback+0x5a/0x70
[   59.136160]  [<c111007b>] ? unuse_mm+0x1db/0x420
[   59.136160]  [<c1073da1>] ? SyS_stime+0x1/0x60
[   59.136160]  [<c14575c4>] ? syscall_call+0x7/0x7
[   59.262230] CPU: 0 PID: 295 Comm: dump_stack_test Not tainted 3.19.8-yocto-standard #1
[   59.270046] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[   59.270046]  00000126 00000126 ce665f3c c1453ba1 ce665f48 c124cea9 cd7d291c ce665f60
[   59.270046]  c10a2762 cd7d2924 00000246 4c089d40 594b2ecf ce665f6c c102853a b777f2e4
[   59.270046]  ce664000 d286209d b777f2e4 b777f2e8 000000e0 4c089d40 594b2ecf ce664000
[   59.270046] Call Trace:
[   59.270046]  [<c1453ba1>] dump_stack+0x16/0x18
[   59.270046]  [<c124cea9>] dumpstack_handler_pre+0x49/0x60
[   59.270046]  [<c10a2762>] opt_pre_handler+0x32/0x60
[   59.270046]  [<c102853a>] optimized_callback+0x5a/0x70
[   59.270046]  [<c111007b>] ? unuse_mm+0x1db/0x420
[   59.270046]  [<c1073da1>] ? SyS_stime+0x1/0x60
[   59.270046]  [<c14575c4>] ? syscall_call+0x7/0x7
[   61.347053] CPU: 0 PID: 296 Comm: dump_stack_test Not tainted 3.19.8-yocto-standard #1
[   61.355026] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[   61.356896]  00000126 00000126 cd241f3c c1453ba1 cd241f48 c124cea9 cd7d2034 cd241f60
[   61.356896]  c10a2762 cd7d203c 00000246 4c089d40 594b2ecf cd241f6c c102853a b6f7f2ec
[   61.356896]  cd240000 d286201b b6f7f2ec b6f7f2e8 000000e0 4c089d40 594b2ecf cd240000


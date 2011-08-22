	.file	"cpu_hog_cbs.c"
.globl __udivdi3
.globl __umoddi3
	.text
	.p2align 4,,15
	.type	to_utility_time, @function
to_utility_time:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$40, %esp
	cmpl	$1, %ecx
	movl	%ebx, -12(%ebp)
	movl	%eax, %ebx
	movl	%esi, -8(%ebp)
	movl	%edx, %esi
	movl	%edi, -4(%ebp)
	movl	8(%ebp), %edi
	je	.L4
	jae	.L9
	movl	%eax, (%edi)
	movl	$0, 4(%edi)
.L7:
	movl	-12(%ebp), %ebx
	movl	-8(%ebp), %esi
	movl	-4(%ebp), %edi
	movl	%ebp, %esp
	popl	%ebp
	ret
	.p2align 4,,7
	.p2align 3
.L9:
	cmpl	$2, %ecx
	je	.L5
	cmpl	$3, %ecx
	jne	.L7
	movl	$1000000000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%eax, (%esp)
	movl	%edx, 4(%esp)
	call	__udivdi3
	movl	%eax, (%edi)
	movl	$1000000000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%ebx, (%esp)
	movl	%esi, 4(%esp)
	call	__umoddi3
	movl	%eax, 4(%edi)
	jmp	.L7
	.p2align 4,,7
	.p2align 3
.L4:
	movl	$1000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%eax, (%esp)
	movl	%edx, 4(%esp)
	call	__udivdi3
	movl	%eax, (%edi)
	movl	$1000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%ebx, (%esp)
	movl	%esi, 4(%esp)
	call	__umoddi3
	imull	$1000000, %eax, %eax
	movl	%eax, 4(%edi)
	jmp	.L7
	.p2align 4,,7
	.p2align 3
.L5:
	movl	$1000000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%eax, (%esp)
	movl	%edx, 4(%esp)
	call	__udivdi3
	movl	%eax, (%edi)
	movl	$1000000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%ebx, (%esp)
	movl	%esi, 4(%esp)
	call	__umoddi3
	imull	$1000, %eax, %eax
	movl	%eax, 4(%edi)
	jmp	.L7
	.size	to_utility_time, .-to_utility_time
	.p2align 4,,15
	.type	utility_time_init, @function
utility_time_init:
	pushl	%ebp
	movl	%esp, %ebp
	movl	$0, (%eax)
	movl	$0, 4(%eax)
	movl	$0, 8(%eax)
	movl	$0, 12(%eax)
	popl	%ebp
	ret
	.size	utility_time_init, .-utility_time_init
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"cpu_hog_cbs.c"
	.section	.rodata.str1.4,"aMS",@progbits,1
	.align 4
.LC1:
	.string	"%s[%lu][%lu][%s@%s:%u]: [ERROR] "
	.align 4
.LC2:
	.string	"You must restore the CPU freq governor yourself\n"
	.text
	.p2align 4,,15
	.type	cleanup_restore_gov, @function
cleanup_restore_gov:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	subl	$52, %esp
	movl	default_gov, %eax
	testl	%eax, %eax
	je	.L14
	movl	%eax, (%esp)
	call	cpu_freq_restore_governor
	testl	%eax, %eax
	jne	.L15
.L14:
	addl	$52, %esp
	popl	%ebx
	popl	%ebp
	ret
	.p2align 4,,7
	.p2align 3
.L15:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	%ebx, 20(%esp)
	movl	$139, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6062, 24(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC1, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	$.LC2, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	funlockfile
	addl	$52, %esp
	popl	%ebx
	popl	%ebp
	ret
	.size	cleanup_restore_gov, .-cleanup_restore_gov
	.section	.rodata.str1.1
.LC3:
	.string	"%lu.%09lu"
.LC4:
	.string	"%d\n%s\n"
	.text
	.p2align 4,,15
	.type	T.84, @function
T.84:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	movl	%eax, %esi
	pushl	%ebx
	subl	$96, %esp
	movl	%gs:20, %eax
	movl	%eax, -12(%ebp)
	xorl	%eax, %eax
	leal	-52(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	$1, (%esp)
	call	clock_gettime
	movl	-52(%ebp), %eax
	movl	t1_abs, %ecx
	movl	$0, -60(%ebp)
	movl	-48(%ebp), %edx
	movl	$0, -56(%ebp)
	cmpl	%ecx, %eax
	je	.L26
	jge	.L27
.L20:
	movl	$0, -68(%ebp)
	xorl	%edx, %edx
	xorl	%eax, %eax
	movl	$0, -64(%ebp)
	jmp	.L19
	.p2align 4,,7
	.p2align 3
.L27:
	movl	t1_abs+4, %ebx
	cmpl	%ebx, %edx
	jl	.L21
.L22:
	subl	%ecx, %eax
	subl	%ebx, %edx
	movl	%eax, -68(%ebp)
	movl	%edx, -64(%ebp)
.L19:
	leal	-44(%ebp), %ebx
	movl	%edx, 24(%esp)
	movl	%eax, 20(%esp)
	movl	$.LC3, 16(%esp)
	movl	$32, 12(%esp)
	movl	$1, 8(%esp)
	movl	$32, 4(%esp)
	movl	%ebx, (%esp)
	call	__snprintf_chk
	movl	%ebx, 12(%esp)
	movl	%esi, 8(%esp)
	movl	$.LC4, 4(%esp)
	movl	$1, (%esp)
	call	__printf_chk
	movl	-12(%ebp), %eax
	xorl	%gs:20, %eax
	jne	.L28
	addl	$96, %esp
	popl	%ebx
	popl	%esi
	popl	%ebp
	ret
	.p2align 4,,7
	.p2align 3
.L21:
	subl	$1, %eax
	addl	$1000000000, %edx
	subl	%ecx, %eax
	subl	%ebx, %edx
	movl	%eax, -68(%ebp)
	movl	%edx, -64(%ebp)
	jmp	.L19
.L28:
	call	__stack_chk_fail
	.p2align 4,,7
	.p2align 3
.L26:
	movl	t1_abs+4, %ebx
	cmpl	%ebx, %edx
	je	.L20
	jl	.L20
	.p2align 4,,4
	jmp	.L22
	.size	T.84, .-T.84
	.p2align 4,,15
	.type	sighand, @function
sighand:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	cmpl	$15, 8(%ebp)
	je	.L32
	leave
	ret
.L32:
	movl	chunk_counter, %eax
	call	T.84
	movl	$0, 4(%esp)
	movl	$jmp_env, (%esp)
	call	__longjmp_chk
	.size	sighand, .-sighand
	.p2align 4,,15
	.type	to_utility_time_dyn, @function
to_utility_time_dyn:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$56, %esp
	movl	$16, (%esp)
	movl	%ebx, -12(%ebp)
	movl	%eax, %ebx
	movl	%esi, -8(%ebp)
	movl	%edi, -4(%ebp)
	movl	%edx, %edi
	movl	%ecx, -28(%ebp)
	call	malloc
	movl	-28(%ebp), %ecx
	testl	%eax, %eax
	movl	%eax, %esi
	je	.L34
	cmpl	$1, %ecx
	movl	$0, (%eax)
	movl	$0, 4(%eax)
	movl	$0, 12(%eax)
	movl	$1, 8(%eax)
	je	.L36
	jae	.L40
	movl	%ebx, (%eax)
	movl	$0, 4(%eax)
	.p2align 4,,7
	.p2align 3
.L34:
	movl	%esi, %eax
	movl	-12(%ebp), %ebx
	movl	-8(%ebp), %esi
	movl	-4(%ebp), %edi
	movl	%ebp, %esp
	popl	%ebp
	ret
	.p2align 4,,7
	.p2align 3
.L40:
	cmpl	$2, %ecx
	je	.L37
	cmpl	$3, %ecx
	jne	.L34
	movl	$1000000000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%ebx, (%esp)
	movl	%edi, 4(%esp)
	call	__udivdi3
	movl	$1000000000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%ebx, (%esp)
	movl	%edi, 4(%esp)
	movl	%eax, (%esi)
	call	__umoddi3
	movl	%eax, 4(%esi)
	jmp	.L34
	.p2align 4,,7
	.p2align 3
.L36:
	movl	$1000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%ebx, (%esp)
	movl	%edi, 4(%esp)
	call	__udivdi3
	movl	$1000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%ebx, (%esp)
	movl	%edi, 4(%esp)
	movl	%eax, (%esi)
	call	__umoddi3
	imull	$1000000, %eax, %eax
	movl	%eax, 4(%esi)
	jmp	.L34
	.p2align 4,,7
	.p2align 3
.L37:
	movl	$1000000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%ebx, (%esp)
	movl	%edi, 4(%esp)
	call	__udivdi3
	movl	$1000000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%ebx, (%esp)
	movl	%edi, 4(%esp)
	movl	%eax, (%esi)
	call	__umoddi3
	imull	$1000, %eax, %eax
	movl	%eax, 4(%esi)
	jmp	.L34
	.size	to_utility_time_dyn, .-to_utility_time_dyn
	.section	.rodata.str1.4
	.align 4
.LC5:
	.string	"%s[%lu][%lu][%s@%s:%u]: [FATAL] "
	.align 4
.LC6:
	.string	"Cannot start experiment (fail to register cleanup_restore_gov at exit)\n (%s)\n"
	.align 4
.LC7:
	.string	"Cannot enter UP mode with maximum frequency (%s)\n"
	.align 4
.LC8:
	.string	"Cannot install SIGTERM handler (%s)\n"
	.align 4
.LC9:
	.ascii	"Usage: %s -e EXEC_TIME -b SLEEP_TIME -q CBS_BUDGET\n       -"
	.ascii	"t CBS_PERIOD [-s STOPPING_TIME]\n\nThis program will run bus"
	.ascii	"yloop chunks, each for the given\nexecution time before slee"
	.ascii	"ping for the stated duration.\nThe busyloop-sleep cycle is d"
	.ascii	"one within CBS having the stated\nbudget and period. If desi"
	.ascii	"red, the program can quit after\nthe total time to perform t"
	.ascii	"he busyloop-sleep cycles exceed\nthe given time. Otherwise, "
	.ascii	"SIGTERM can be sent to quit the\nprogram gracefully. When th"
	.ascii	"e program quits gracefully, it\nprints on stdout the number "
	.ascii	"of cycles fully performed\nfollowed by a newline followed by"
	.ascii	" the elapsed time in second\nsince the first chunk execution"
	.ascii	" followed by a newline.\n-e EXEC_TIME is the busyloop durati"
	.ascii	"on in millisecond.\n   This should be greater than zero.\n-b"
	.ascii	" SLEEP_TIME is the sleeping duration in millisecond.\n   Thi"
	.ascii	"s should be greater than or equal to zero.\n-q CBS_BUDGET is"
	.ascii	" the CBS budget in millisecond.\n   This should be greater t"
	.ascii	"han zero.\n-t CBS_PERIOD is t"
	.string	"he CBS period in millisecond.\n   This should be greater than or equal to CBS_BUDGET.\n-i STOPPING_TIME is the elapsed time in ms since the\n   beginning of the first busyloop-sleep cycle after which\n   this program will quit before beginning the next cycle.\n"
	.align 4
.LC10:
	.string	"Unrecognized option character -%c\n"
	.section	.rodata.str1.1
.LC11:
	.string	"Option -%c needs an argument\n"
	.section	.rodata.str1.4
	.align 4
.LC12:
	.string	"Unexpected return value of fn getopt\n"
	.section	.rodata.str1.1
.LC13:
	.string	":he:b:q:t:s:"
	.section	.rodata.str1.4
	.align 4
.LC14:
	.string	"EXEC_TIME must be properly specified (-h for help)\n"
	.align 4
.LC15:
	.string	"SLEEP_TIME must be properly specified (-h for help)\n"
	.align 4
.LC16:
	.string	"CBS_BUDGET must be properly specified (-h for help)\n"
	.align 4
.LC17:
	.string	"CBS_PERIOD must be properly specified (-h for help)\n"
	.align 4
.LC18:
	.string	"STOPPING_TIME must be at least 1 ms (-h for help)\n"
	.align 4
.LC19:
	.string	"%d is too small for execution busyloop\n"
	.align 4
.LC20:
	.string	"%d is too big for execution busyloop\n"
	.align 4
.LC21:
	.string	"Cannot create execution busyloop\n"
	.align 4
.LC22:
	.string	"Cannot enter SCHED_DEADLINE (privilege may be insufficient)\n"
	.text
	.p2align 4,,15
.globl main
	.type	main, @function
main:
	pushl	%ebp
	movl	%esp, %ebp
	andl	$-16, %esp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$324, %esp
	movl	stderr, %eax
	movl	8(%ebp), %esi
	movl	12(%ebp), %ebx
	movl	$cleanup_restore_gov, (%esp)
	movl	%eax, log_stream
	call	atexit
	testl	%eax, %eax
	jne	.L102
	movl	$default_gov, (%esp)
	call	enter_UP_mode_freq_max
	testl	%eax, %eax
	jne	.L103
	leal	120(%esp), %edi
	movl	$35, %ecx
	rep stosl
	leal	120(%esp), %eax
	movl	$sighand, 120(%esp)
	movl	$0, 8(%esp)
	movl	%eax, 4(%esp)
	movl	$15, (%esp)
	call	sigaction
	testl	%eax, %eax
	jne	.L104
	movl	$0, opterr
	movl	$-1, %edi
	movl	$-1, 104(%esp)
	movl	$-1, 96(%esp)
	movl	$-1, 100(%esp)
	movl	$-1, 108(%esp)
.L98:
	movl	$.LC13, 8(%esp)
	movl	%ebx, 4(%esp)
	movl	%esi, (%esp)
	call	getopt
	cmpl	$-1, %eax
	je	.L105
	subl	$58, %eax
	cmpl	$58, %eax
	jbe	.L106
.L46:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$207, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	$.LC12, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
.L101:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	funlockfile
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	fflush
	movl	$1, (%esp)
	call	exit
	.p2align 4,,7
	.p2align 3
.L106:
	jmp	*.L55(,%eax,4)
	.section	.rodata
	.align 4
	.align 4
.L55:
	.long	.L47
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L48
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L49
	.long	.L46
	.long	.L46
	.long	.L50
	.long	.L46
	.long	.L46
	.long	.L51
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L46
	.long	.L52
	.long	.L46
	.long	.L53
	.long	.L54
	.text
.L54:
	movl	optarg, %eax
	movl	$10, 8(%esp)
	movl	$0, 4(%esp)
	movl	%eax, (%esp)
	call	strtol
	movl	%eax, 96(%esp)
	jmp	.L98
.L53:
	movl	optarg, %eax
	movl	$10, 8(%esp)
	movl	$0, 4(%esp)
	movl	%eax, (%esp)
	call	strtol
	movl	%eax, 104(%esp)
	jmp	.L98
.L52:
	movl	optarg, %eax
	movl	$10, 8(%esp)
	movl	$0, 4(%esp)
	movl	%eax, (%esp)
	call	strtol
	movl	%eax, 100(%esp)
	jmp	.L98
.L51:
	movl	$prog_name, 8(%esp)
	movl	$.LC9, 4(%esp)
	movl	$1, (%esp)
	call	__printf_chk
.L56:
	addl	$324, %esp
	xorl	%eax, %eax
	popl	%ebx
	popl	%esi
	popl	%edi
	movl	%ebp, %esp
	popl	%ebp
	ret
.L50:
	movl	optarg, %eax
	movl	$10, 8(%esp)
	movl	$0, 4(%esp)
	movl	%eax, (%esp)
	call	strtol
	movl	%eax, %edi
	jmp	.L98
.L49:
	movl	optarg, %eax
	movl	$10, 8(%esp)
	movl	$0, 4(%esp)
	movl	%eax, (%esp)
	call	strtol
	movl	%eax, 108(%esp)
	jmp	.L98
.L48:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$203, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	optopt, %eax
	movl	$.LC10, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, 12(%esp)
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L47:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$205, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	optopt, %eax
	movl	$.LC11, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, 12(%esp)
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L105:
	testl	%edi, %edi
	jle	.L107
	movl	108(%esp), %ecx
	testl	%ecx, %ecx
	.p2align 4,,4
	js	.L108
	leal	260(%esp), %ebx
	movl	%ebx, %eax
	call	utility_time_init
	movl	108(%esp), %eax
	movl	$1, %ecx
	movl	%ebx, (%esp)
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%eax, 64(%esp)
	movl	%edx, 68(%esp)
	call	to_utility_time
	movl	260(%esp), %eax
	movl	100(%esp), %edx
	movl	%eax, 308(%esp)
	movl	264(%esp), %eax
	testl	%edx, %edx
	movl	%eax, 312(%esp)
	jle	.L109
	movl	96(%esp), %edx
	cmpl	%edx, 100(%esp)
	jg	.L110
	movl	104(%esp), %eax
	cmpl	$-1, 104(%esp)
	setne	91(%esp)
	testl	%eax, %eax
	jle	.L111
.L62:
	leal	276(%esp), %ebx
	movl	%ebx, %eax
	call	utility_time_init
	movl	104(%esp), %eax
	movl	$1, %ecx
	movl	%ebx, (%esp)
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%eax, 56(%esp)
	movl	%edx, 60(%esp)
	call	to_utility_time
	xorl	%edx, %edx
	movl	$2, %ecx
	movl	$100, %eax
	movl	$0, 316(%esp)
	call	to_utility_time_dyn
	movl	%edi, %edx
	movl	$1, %ecx
	sarl	$31, %edx
	movl	%eax, %ebx
	movl	%edi, %eax
	call	to_utility_time_dyn
	leal	316(%esp), %edx
	movl	%edx, 16(%esp)
	movl	$10, 12(%esp)
	movl	%ebx, 8(%esp)
	movl	$0, (%esp)
	movl	%eax, 4(%esp)
	call	create_cpu_busyloop
	cmpl	$-2, %eax
	je	.L112
	cmpl	$-4, %eax
	je	.L113
	testl	%eax, %eax
	.p2align 4,,5
	jne	.L114
	movl	96(%esp), %eax
	movl	$1, %ecx
	movl	%eax, %edx
	sarl	$31, %edx
	call	to_utility_time_dyn
	movl	$1, %ecx
	movl	%eax, %ebx
	movl	100(%esp), %eax
	movl	%eax, %edx
	sarl	$31, %edx
	call	to_utility_time_dyn
	movl	$0, 8(%esp)
	movl	%ebx, 4(%esp)
	movl	%eax, (%esp)
	call	sched_deadline_enter
	testl	%eax, %eax
	jne	.L115
	movl	$0, 4(%esp)
	movl	$jmp_env, (%esp)
	call	__sigsetjmp
	testl	%eax, %eax
	jne	.L67
	cmpb	$0, 91(%esp)
	leal	276(%esp), %edx
	movl	316(%esp), %ebx
	cmove	%eax, %edx
	cmpl	$0, 108(%esp)
	movl	%edx, 92(%esp)
	je	.L70
	movl	$t1_abs, %edx
	movl	$4, %ecx
	movl	%edx, %edi
	rep stosl
	leal	260(%esp), %edx
	cmpl	$0, 92(%esp)
	movl	%edx, %edi
	movb	$4, %cl
	rep stosl
	je	.L116
	leal	300(%esp), %eax
	xorl	%esi, %esi
	movl	%edx, 84(%esp)
	movl	%eax, 4(%esp)
	movl	$1, (%esp)
	call	clock_gettime
	movl	300(%esp), %eax
	movl	84(%esp), %edx
	movl	%eax, t1_abs
	movl	304(%esp), %eax
	movl	%edx, 76(%esp)
	movl	%eax, t1_abs+4
.L96:
	movl	12(%ebx), %eax
#APP
# 183 "../utility_cpu.h" 1
	0:
	addl $1, %esi
	cmpl %eax, %esi
	jne 0b
# 0 "" 2
#NO_APP
	leal	308(%esp), %eax
	movl	$t1_abs, %edi
	movl	%esi, 12(%esp)
	movl	%eax, 8(%esp)
	movl	%esi, 4(%esp)
	movl	$1, (%esp)
	call	clock_nanosleep
	leal	292(%esp), %edx
	addl	$1, chunk_counter
	movl	%edx, 4(%esp)
	movl	$1, (%esp)
	call	clock_gettime
	movl	292(%esp), %eax
	movl	(%edi), %edx
	movl	296(%esp), %ecx
	cmpl	%edx, %eax
	je	.L117
	jge	.L118
.L83:
	movl	76(%esp), %edi
	movl	%esi, %eax
	movl	$2, %ecx
	rep stosl
	xorl	%eax, %eax
.L75:
	movl	92(%esp), %edx
	cmpl	%eax, (%edx)
	jg	.L96
	je	.L119
.L77:
	movl	chunk_counter, %eax
	call	T.84
.L67:
	movl	316(%esp), %eax
	movl	%eax, (%esp)
	call	destroy_cpu_busyloop
	jmp	.L56
.L118:
	cmpl	t1_abs+4, %ecx
	jge	.L85
	addl	$1000000000, %ecx
	subl	$1, %eax
	subl	t1_abs+4, %ecx
	subl	%edx, %eax
	movl	%eax, 260(%esp)
	movl	%ecx, 264(%esp)
	jmp	.L75
.L117:
	cmpl	t1_abs+4, %ecx
	je	.L83
	jl	.L83
.L85:
	subl	t1_abs+4, %ecx
	subl	%edx, %eax
	movl	%eax, 260(%esp)
	movl	%ecx, 264(%esp)
	jmp	.L75
.L119:
	movl	264(%esp), %eax
	cmpl	4(%edx), %eax
	jge	.L77
	jmp	.L96
.L70:
	movl	108(%esp), %eax
	movl	$t1_abs, %edx
	movl	$4, %ecx
	movl	%edx, %edi
	cmpl	$0, 92(%esp)
	leal	260(%esp), %esi
	rep stosl
	movl	%esi, %edi
	movb	$4, %cl
	rep stosl
	je	.L120
	leal	300(%esp), %eax
	movl	%eax, 4(%esp)
	movl	$1, (%esp)
	call	clock_gettime
	movl	300(%esp), %eax
	leal	292(%esp), %edx
	movl	%esi, 72(%esp)
	movl	92(%esp), %esi
	movl	%eax, t1_abs
	movl	304(%esp), %eax
	movl	%eax, t1_abs+4
.L97:
	movl	12(%ebx), %ecx
	xorl	%eax, %eax
#APP
# 183 "../utility_cpu.h" 1
	0:
	addl $1, %eax
	cmpl %ecx, %eax
	jne 0b
# 0 "" 2
#NO_APP
	addl	$1, chunk_counter
	movl	%edx, 4(%esp)
	movl	%edx, 84(%esp)
	movl	$1, (%esp)
	call	clock_gettime
	movl	292(%esp), %eax
	movl	t1_abs, %edi
	movl	296(%esp), %ecx
	movl	84(%esp), %edx
	cmpl	%edi, %eax
	je	.L121
	jge	.L122
.L86:
	movl	72(%esp), %edi
	movl	$2, %ecx
	xorl	%eax, %eax
	rep stosl
.L81:
	cmpl	%eax, (%esi)
	jg	.L97
	jne	.L77
	movl	264(%esp), %eax
	cmpl	4(%esi), %eax
	jge	.L77
	.p2align 4,,3
	jmp	.L97
.L122:
	cmpl	t1_abs+4, %ecx
	jl	.L123
.L88:
	subl	t1_abs+4, %ecx
	subl	%edi, %eax
	movl	%eax, 260(%esp)
	movl	%ecx, 264(%esp)
	jmp	.L81
.L121:
	cmpl	t1_abs+4, %ecx
	je	.L86
	jl	.L86
	.p2align 4,,6
	jmp	.L88
.L123:
	addl	$1000000000, %ecx
	subl	$1, %eax
	subl	t1_abs+4, %ecx
	subl	%edi, %eax
	movl	%eax, 260(%esp)
	movl	%ecx, 264(%esp)
	jmp	.L81
.L116:
	leal	300(%esp), %eax
	xorl	%esi, %esi
	movl	%eax, 4(%esp)
	leal	308(%esp), %edi
	movl	$1, (%esp)
	call	clock_gettime
	movl	300(%esp), %eax
	movl	%eax, t1_abs
	movl	304(%esp), %eax
	movl	%eax, t1_abs+4
	.p2align 4,,7
	.p2align 3
.L72:
	movl	12(%ebx), %eax
#APP
# 183 "../utility_cpu.h" 1
	0:
	addl $1, %esi
	cmpl %eax, %esi
	jne 0b
# 0 "" 2
#NO_APP
	movl	%esi, 12(%esp)
	movl	%edi, 8(%esp)
	movl	%esi, 4(%esp)
	movl	$1, (%esp)
	call	clock_nanosleep
	addl	$1, chunk_counter
	jmp	.L72
.L120:
	leal	300(%esp), %eax
	movl	%eax, 4(%esp)
	movl	$1, (%esp)
	call	clock_gettime
	movl	300(%esp), %eax
	movl	%eax, t1_abs
	movl	304(%esp), %eax
	movl	%eax, t1_abs+4
	movl	12(%ebx), %edx
	xorl	%eax, %eax
	.p2align 4,,7
	.p2align 3
.L78:
#APP
# 183 "../utility_cpu.h" 1
	0:
	addl $1, %eax
	cmpl %edx, %eax
	jne 0b
# 0 "" 2
#NO_APP
	jmp	.L78
.L115:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$258, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	$.LC22, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L104:
	call	__errno_location
	movl	(%eax), %eax
	movl	%eax, (%esp)
	call	strerror
	movl	%eax, %ebx
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	$145, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%esi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	%ebx, 12(%esp)
	movl	$.LC8, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L103:
	call	__errno_location
	movl	(%eax), %eax
	movl	%eax, (%esp)
	call	strerror
	movl	%eax, %ebx
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	$139, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%esi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	%ebx, 12(%esp)
	movl	$.LC7, 8(%esp)
.L100:
	movl	log_stream, %eax
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L102:
	call	__errno_location
	movl	(%eax), %eax
	movl	%eax, (%esp)
	call	strerror
	movl	%eax, %ebx
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	$139, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%esi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	%ebx, 12(%esp)
	movl	$.LC6, 8(%esp)
	jmp	.L100
.L114:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$250, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	$.LC21, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L113:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$248, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	%edi, 12(%esp)
	movl	$.LC20, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L112:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$246, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	%edi, 12(%esp)
	movl	$.LC19, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L111:
	cmpb	$0, 91(%esp)
	je	.L62
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$229, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	$.LC18, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L110:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$226, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	$.LC17, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L109:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$223, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	$.LC16, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L108:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$215, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	$.LC15, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
.L107:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	$212, 32(%esp)
	movl	$.LC0, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	$.LC14, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	jmp	.L101
	.size	main, .-main
.globl prog_name
	.section	.rodata
	.type	prog_name, @object
	.size	prog_name, 12
prog_name:
	.string	"cpu_hog_cbs"
	.local	default_gov
	.comm	default_gov,4,4
	.local	chunk_counter
	.comm	chunk_counter,4,4
	.type	__FUNCTION__.6100, @object
	.size	__FUNCTION__.6100, 5
__FUNCTION__.6100:
	.string	"main"
	.type	__FUNCTION__.6062, @object
	.size	__FUNCTION__.6062, 20
__FUNCTION__.6062:
	.string	"cleanup_restore_gov"
	.local	t1_abs
	.comm	t1_abs,16,4
	.local	jmp_env
	.comm	jmp_env,156,32
	.comm	log_stream,4,4
	.ident	"GCC: (Ubuntu/Linaro 4.4.4-14ubuntu5) 4.4.5"
	.section	.note.GNU-stack,"",@progbits

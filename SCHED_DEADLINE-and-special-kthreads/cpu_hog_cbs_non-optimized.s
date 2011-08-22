	.file	"cpu_hog_cbs.c"
.globl __udivdi3
.globl __umoddi3
	.text
	.p2align 4,,-1
	.type	to_utility_time, @function
to_utility_time:
	subl	$28, %esp
	cmpl	$1, %ecx
	movl	%ebx, 16(%esp)
	movl	%eax, %ebx
	movl	%esi, 20(%esp)
	movl	%edx, %esi
	movl	%edi, 24(%esp)
	movl	32(%esp), %edi
	je	.L4
	jae	.L9
	movl	%eax, (%edi)
	movl	$0, 4(%edi)
.L7:
	movl	16(%esp), %ebx
	movl	20(%esp), %esi
	movl	24(%esp), %edi
	addl	$28, %esp
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
	movl	%ebx, (%esp)
	movl	%esi, 4(%esp)
	movl	$1000000000, 8(%esp)
	movl	$0, 12(%esp)
	call	__umoddi3
	movl	%eax, 4(%edi)
	movl	16(%esp), %ebx
	movl	20(%esp), %esi
	movl	24(%esp), %edi
	addl	$28, %esp
	ret
	.p2align 4,,7
	.p2align 3
.L4:
	movl	$1000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%eax, (%esp)
	movl	%edx, 4(%esp)
	call	__udivdi3
	movl	%eax, (%edi)
	movl	%ebx, (%esp)
	movl	%esi, 4(%esp)
	movl	$1000, 8(%esp)
	movl	$0, 12(%esp)
	call	__umoddi3
	imull	$1000000, %eax, %eax
	movl	%eax, 4(%edi)
	movl	16(%esp), %ebx
	movl	20(%esp), %esi
	movl	24(%esp), %edi
	addl	$28, %esp
	ret
	.p2align 4,,7
	.p2align 3
.L5:
	movl	$1000000, 8(%esp)
	movl	$0, 12(%esp)
	movl	%eax, (%esp)
	movl	%edx, 4(%esp)
	call	__udivdi3
	movl	%eax, (%edi)
	movl	%ebx, (%esp)
	movl	%esi, 4(%esp)
	movl	$1000000, 8(%esp)
	movl	$0, 12(%esp)
	call	__umoddi3
	imull	$1000, %eax, %edx
	movl	%edx, 4(%edi)
	movl	16(%esp), %ebx
	movl	20(%esp), %esi
	movl	24(%esp), %edi
	addl	$28, %esp
	ret
	.size	to_utility_time, .-to_utility_time
	.p2align 4,,-1
	.type	timespec_to_utility_time, @function
timespec_to_utility_time:
	movl	(%eax), %ecx
	movl	4(%eax), %eax
	movl	%ecx, (%edx)
	movl	%eax, 4(%edx)
	ret
	.size	timespec_to_utility_time, .-timespec_to_utility_time
	.p2align 4,,-1
	.type	to_timespec, @function
to_timespec:
	movl	(%eax), %ecx
	movl	4(%eax), %eax
	movl	%ecx, (%edx)
	movl	%eax, 4(%edx)
	ret
	.size	to_timespec, .-to_timespec
	.p2align 4,,-1
	.type	utility_time_eq, @function
utility_time_eq:
	pushl	%ebx
	movl	(%eax), %ebx
	xorl	%ecx, %ecx
	cmpl	(%edx), %ebx
	je	.L18
	movl	%ecx, %eax
	popl	%ebx
	ret
	.p2align 4,,7
	.p2align 3
.L18:
	movl	4(%eax), %eax
	xorl	%ecx, %ecx
	cmpl	4(%edx), %eax
	popl	%ebx
	sete	%cl
	movl	%ecx, %eax
	ret
	.size	utility_time_eq, .-utility_time_eq
	.p2align 4,,-1
	.type	utility_time_lt, @function
utility_time_lt:
	pushl	%ebx
	movl	(%edx), %ebx
	movl	$1, %ecx
	cmpl	%ebx, (%eax)
	jl	.L21
	je	.L22
	xorb	%cl, %cl
.L21:
	movl	%ecx, %eax
	popl	%ebx
	ret
	.p2align 4,,7
	.p2align 3
.L22:
	movl	4(%eax), %eax
	xorl	%ecx, %ecx
	cmpl	4(%edx), %eax
	popl	%ebx
	setl	%cl
	movl	%ecx, %eax
	ret
	.size	utility_time_lt, .-utility_time_lt
	.p2align 4,,-1
	.type	utility_time_le, @function
utility_time_le:
	subl	$8, %esp
	movl	%ebx, (%esp)
	movl	%eax, %ebx
	movl	%esi, 4(%esp)
	movl	%edx, %esi
	call	utility_time_eq
	movl	%eax, %edx
	movl	$1, %eax
	testl	%edx, %edx
	jne	.L26
	movl	%esi, %edx
	movl	%ebx, %eax
	call	utility_time_lt
	testl	%eax, %eax
	setne	%al
	movzbl	%al, %eax
.L26:
	movl	(%esp), %ebx
	movl	4(%esp), %esi
	addl	$8, %esp
	ret
	.size	utility_time_le, .-utility_time_le
	.p2align 4,,-1
	.type	utility_time_ge, @function
utility_time_ge:
	call	utility_time_lt
	testl	%eax, %eax
	sete	%al
	movzbl	%al, %eax
	ret
	.size	utility_time_ge, .-utility_time_ge
	.p2align 4,,-1
	.type	busyloop, @function
busyloop:
	xorl	%edx, %edx
#APP
# 183 "../utility_cpu.h" 1
	0:
	addl $1, %edx
	cmpl %eax, %edx
	jne 0b
# 0 "" 2
#NO_APP
	ret
	.size	busyloop, .-busyloop
	.p2align 4,,-1
	.type	keep_cpu_busy, @function
keep_cpu_busy:
	movl	12(%eax), %eax
	jmp	busyloop
	.size	keep_cpu_busy, .-keep_cpu_busy
	.p2align 4
	.type	busyloop_sleep_cycle_no_break, @function
busyloop_sleep_cycle_no_break:
	subl	$4, %esp
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	keep_cpu_busy
	movl	12(%esp), %eax
	movl	(%eax), %eax
	leal	1(%eax), %edx
	movl	12(%esp), %eax
	movl	%edx, (%eax)
	addl	$4, %esp
	ret
	.size	busyloop_sleep_cycle_no_break, .-busyloop_sleep_cycle_no_break
	.p2align 4,,-1
	.type	utility_time_init, @function
utility_time_init:
	movl	$0, (%eax)
	movl	$0, 4(%eax)
	movl	$0, 8(%eax)
	movl	$0, 12(%eax)
	ret
	.size	utility_time_init, .-utility_time_init
	.p2align 4,,-1
	.type	utility_time_init_dyn, @function
utility_time_init_dyn:
	pushl	%ebx
	movl	%eax, %ebx
	call	utility_time_init
	movl	$1, 8(%ebx)
	popl	%ebx
	ret
	.size	utility_time_init_dyn, .-utility_time_init_dyn
	.p2align 4,,-1
	.type	utility_time_zero_t, @function
utility_time_zero_t:
	movl	$0, (%eax)
	movl	$0, 4(%eax)
	ret
	.size	utility_time_zero_t, .-utility_time_zero_t
	.p2align 4,,-1
	.type	utility_time_sub, @function
utility_time_sub:
	subl	$12, %esp
	movl	%ebx, (%esp)
	movl	%edx, %ebx
	movl	%esi, 4(%esp)
	movl	%eax, %esi
	movl	%edi, 8(%esp)
	movl	%ecx, %edi
	call	utility_time_le
	testl	%eax, %eax
	jne	.L47
	movl	4(%esi), %eax
	cmpl	4(%ebx), %eax
	jl	.L48
	movl	(%esi), %ecx
	subl	(%ebx), %ecx
	subl	4(%ebx), %eax
	movl	%ecx, (%edi)
	movl	%eax, 4(%edi)
	movl	(%esp), %ebx
	movl	4(%esp), %esi
	movl	8(%esp), %edi
	addl	$12, %esp
	ret
	.p2align 4,,7
	.p2align 3
.L48:
	movl	(%esi), %edx
	addl	$1000000000, %eax
	subl	4(%ebx), %eax
	subl	$1, %edx
	subl	(%ebx), %edx
	movl	%eax, 4(%edi)
	movl	%edx, (%edi)
	movl	(%esp), %ebx
	movl	4(%esp), %esi
	movl	8(%esp), %edi
	addl	$12, %esp
	ret
	.p2align 4,,7
	.p2align 3
.L47:
	movl	%edi, %eax
	movl	(%esp), %ebx
	movl	4(%esp), %esi
	movl	8(%esp), %edi
	addl	$12, %esp
	jmp	utility_time_zero_t
	.size	utility_time_sub, .-utility_time_sub
	.p2align 4,,-1
	.type	utility_time_make_dyn, @function
utility_time_make_dyn:
	pushl	%ebx
	subl	$24, %esp
	movl	$16, (%esp)
	call	malloc
	testl	%eax, %eax
	movl	%eax, %ebx
	je	.L50
	call	utility_time_init_dyn
.L50:
	movl	%ebx, %eax
	addl	$24, %esp
	popl	%ebx
	ret
	.size	utility_time_make_dyn, .-utility_time_make_dyn
	.p2align 4,,-1
	.type	to_utility_time_dyn, @function
to_utility_time_dyn:
	subl	$44, %esp
	movl	%ebx, 32(%esp)
	movl	%eax, %ebx
	movl	%esi, 36(%esp)
	movl	%edx, %esi
	movl	%edi, 40(%esp)
	movl	%ecx, 28(%esp)
	call	utility_time_make_dyn
	movl	28(%esp), %ecx
	testl	%eax, %eax
	movl	%eax, %edi
	je	.L53
	movl	%eax, (%esp)
	movl	%esi, %edx
	movl	%ebx, %eax
	call	to_utility_time
.L53:
	movl	%edi, %eax
	movl	32(%esp), %ebx
	movl	36(%esp), %esi
	movl	40(%esp), %edi
	addl	$44, %esp
	ret
	.size	to_utility_time_dyn, .-to_utility_time_dyn
	.p2align 4,,-1
	.type	break_at_stopping_time, @function
break_at_stopping_time:
	subl	$60, %esp
	movl	%ebx, 48(%esp)
	movl	%ecx, %ebx
	movl	%esi, 52(%esp)
	leal	40(%esp), %esi
	movl	%edi, 56(%esp)
	movl	%edx, %edi
	movl	%esi, 4(%esp)
	movl	%eax, 28(%esp)
	movl	$1, (%esp)
	call	clock_gettime
	movl	%ebx, %edx
	movl	%esi, %eax
	call	timespec_to_utility_time
	movl	%ebx, %ecx
	movl	%edi, %edx
	movl	%ebx, %eax
	call	utility_time_sub
	movl	28(%esp), %edx
	movl	%ebx, %eax
	call	utility_time_ge
	movl	48(%esp), %ebx
	movl	52(%esp), %esi
	movl	56(%esp), %edi
	addl	$60, %esp
	ret
	.size	break_at_stopping_time, .-break_at_stopping_time
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"%lu.%09lu"
	.text
	.p2align 4,,-1
	.type	to_string, @function
to_string:
	pushl	%ebx
	movl	%ecx, %ebx
	subl	$40, %esp
	movl	4(%eax), %ecx
	movl	%ecx, 24(%esp)
	movl	(%eax), %eax
	movl	%ebx, 4(%esp)
	movl	%edx, (%esp)
	movl	$.LC0, 16(%esp)
	movl	%eax, 20(%esp)
	movl	$-1, 12(%esp)
	movl	$1, 8(%esp)
	call	__snprintf_chk
	movl	%eax, %edx
	leal	1(%eax), %ecx
	xorl	%eax, %eax
	cmpl	%edx, %ebx
	cmovbe	%ecx, %eax
	addl	$40, %esp
	popl	%ebx
	ret
	.size	to_string, .-to_string
	.section	.rodata.str1.1
.LC1:
	.string	"%d\n%s\n"
	.text
	.p2align 4,,-1
	.type	print_stats, @function
print_stats:
	subl	$108, %esp
	movl	%ebx, 96(%esp)
	leal	36(%esp), %ebx
	movl	%esi, 100(%esp)
	leal	52(%esp), %esi
	movl	%edi, 104(%esp)
	movl	%edx, %edi
	movl	%eax, 28(%esp)
	movl	%esi, 4(%esp)
	movl	%gs:20, %eax
	movl	%eax, 92(%esp)
	xorl	%eax, %eax
	movl	$1, (%esp)
	call	clock_gettime
	movl	%ebx, %eax
	call	utility_time_init
	movl	%esi, %eax
	movl	%ebx, %edx
	call	timespec_to_utility_time
	movl	28(%esp), %edx
	movl	%ebx, %ecx
	movl	%ebx, %eax
	leal	60(%esp), %esi
	call	utility_time_sub
	movl	$32, %ecx
	movl	%esi, %edx
	movl	%ebx, %eax
	call	to_string
	movl	%esi, 12(%esp)
	movl	%edi, 8(%esp)
	movl	$.LC1, 4(%esp)
	movl	$1, (%esp)
	call	__printf_chk
	movl	92(%esp), %eax
	xorl	%gs:20, %eax
	jne	.L64
	movl	96(%esp), %ebx
	movl	100(%esp), %esi
	movl	104(%esp), %edi
	addl	$108, %esp
	ret
.L64:
	call	__stack_chk_fail
	.size	print_stats, .-print_stats
	.p2align 4,,-1
	.type	sighand, @function
sighand:
	subl	$28, %esp
	cmpl	$15, 32(%esp)
	je	.L68
	addl	$28, %esp
	ret
.L68:
	movl	chunk_counter, %edx
	movl	$t1_abs, %eax
	call	print_stats
	movl	$0, 4(%esp)
	movl	$jmp_env, (%esp)
	call	__longjmp_chk
	.size	sighand, .-sighand
	.section	.rodata.str1.1
.LC2:
	.string	"cpu_hog_cbs.c"
	.section	.rodata.str1.4,"aMS",@progbits,1
	.align 4
.LC3:
	.string	"%s[%lu][%lu][%s@%s:%u]: [ERROR] "
	.align 4
.LC4:
	.string	"You must restore the CPU freq governor yourself\n"
	.text
	.p2align 4,,-1
	.type	cleanup_restore_gov, @function
cleanup_restore_gov:
	pushl	%ebx
	subl	$56, %esp
	movl	default_gov, %eax
	testl	%eax, %eax
	je	.L71
	movl	%eax, (%esp)
	call	cpu_freq_restore_governor
	testl	%eax, %eax
	jne	.L72
.L71:
	addl	$56, %esp
	popl	%ebx
	.p2align 4,,1
	ret
	.p2align 4,,7
	.p2align 3
.L72:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	log_stream, %ecx
	movl	%ebx, 20(%esp)
	movl	$139, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	%ecx, (%esp)
	movl	$__FUNCTION__.6062, 24(%esp)
	movl	%eax, 16(%esp)
	movl	$prog_name, 12(%esp)
	movl	$.LC3, 8(%esp)
	movl	$1, 4(%esp)
	call	__fprintf_chk
	movl	log_stream, %edx
	movl	$.LC4, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edx, (%esp)
	call	__fprintf_chk
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	funlockfile
	addl	$56, %esp
	popl	%ebx
	ret
	.size	cleanup_restore_gov, .-cleanup_restore_gov
	.p2align 4
	.type	busyloop_sleep_cycle_with_break, @function
busyloop_sleep_cycle_with_break:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	movl	8(%ebp), %eax
	movl	%eax, (%esp)
	call	keep_cpu_busy
	movl	$0, 12(%esp)
	movl	16(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	$0, 4(%esp)
	movl	$1, (%esp)
	call	clock_nanosleep
	movl	12(%ebp), %eax
	movl	(%eax), %eax
	leal	1(%eax), %edx
	movl	12(%ebp), %eax
	movl	%edx, (%eax)
	leave
	ret
	.size	busyloop_sleep_cycle_with_break, .-busyloop_sleep_cycle_with_break
	.p2align 4,,-1
	.type	hog_cpu, @function
hog_cpu:
	pushl	%edi
	movl	%eax, %edi
	pushl	%esi
	pushl	%ebx
	movl	%ecx, %ebx
	subl	$64, %esp
	movl	80(%esp), %eax
	movl	%edx, 28(%esp)
	movl	84(%esp), %esi
	call	utility_time_init
	leal	40(%esp), %eax
	call	utility_time_init
	testl	%edi, %edi
	je	.L76
	cmpl	$0, 28(%esp)
	je	.L86
	leal	56(%esp), %edx
	movl	%edx, 4(%esp)
	movl	%edx, 24(%esp)
	movl	$1, (%esp)
	call	clock_gettime
	movl	24(%esp), %ecx
	movl	80(%esp), %edx
	movl	%ecx, %eax
	call	timespec_to_utility_time
.L80:
	movl	%edi, 8(%esp)
	movl	%esi, 4(%esp)
	movl	%ebx, (%esp)
	call	busyloop_sleep_cycle_with_break
	movl	80(%esp), %edx
	leal	40(%esp), %ecx
	movl	28(%esp), %eax
	call	break_at_stopping_time
	testl	%eax, %eax
	je	.L80
.L84:
	addl	$64, %esp
	popl	%ebx
	popl	%esi
	popl	%edi
	ret
.L76:
	cmpl	$0, 28(%esp)
	.p2align 4,,2
	je	.L87
	leal	56(%esp), %edi
	movl	%edi, 4(%esp)
	movl	$1, (%esp)
	call	clock_gettime
	movl	80(%esp), %edx
	movl	%edi, %eax
	call	timespec_to_utility_time
.L83:
	movl	%esi, 4(%esp)
	movl	%ebx, (%esp)
	call	busyloop_sleep_cycle_no_break
	movl	80(%esp), %edx
	leal	40(%esp), %ecx
	movl	28(%esp), %eax
	call	break_at_stopping_time
	testl	%eax, %eax
	je	.L83
	jmp	.L84
.L86:
	leal	56(%esp), %eax
	movl	%eax, 4(%esp)
	movl	$1, (%esp)
	movl	%eax, 24(%esp)
	call	clock_gettime
	movl	24(%esp), %ecx
	movl	80(%esp), %edx
	movl	%ecx, %eax
	call	timespec_to_utility_time
	.p2align 4,,7
	.p2align 3
.L78:
	movl	%edi, 8(%esp)
	movl	%esi, 4(%esp)
	movl	%ebx, (%esp)
	call	busyloop_sleep_cycle_with_break
	jmp	.L78
.L87:
	leal	56(%esp), %edi
	movl	%edi, 4(%esp)
	movl	$1, (%esp)
	call	clock_gettime
	movl	80(%esp), %edx
	movl	%edi, %eax
	call	timespec_to_utility_time
	.p2align 4,,7
	.p2align 3
.L82:
	movl	%esi, 4(%esp)
	movl	%ebx, (%esp)
	call	busyloop_sleep_cycle_no_break
	jmp	.L82
	.size	hog_cpu, .-hog_cpu
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
	.p2align 4,,-1
.globl main
	.type	main, @function
main:
	pushl	%ebp
	movl	%esp, %ebp
	andl	$-16, %esp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$276, %esp
	movl	stderr, %eax
	movl	12(%ebp), %ebx
	movl	$cleanup_restore_gov, (%esp)
	movl	%eax, log_stream
	call	atexit
	testl	%eax, %eax
	jne	.L123
	movl	$default_gov, (%esp)
	call	enter_UP_mode_freq_max
	testl	%eax, %eax
	jne	.L124
	leal	88(%esp), %esi
	movl	$35, %ecx
	movl	%esi, %edi
	rep stosl
	movl	$sighand, 88(%esp)
	movl	$0, 8(%esp)
	movl	%esi, 4(%esp)
	movl	$15, (%esp)
	call	sigaction
	testl	%eax, %eax
	jne	.L125
	movl	$0, opterr
	movl	$-1, %esi
	movl	$-1, %edi
	movl	$-1, 68(%esp)
	movl	$-1, 76(%esp)
	movl	$-1, 72(%esp)
.L120:
	movl	8(%ebp), %eax
	movl	$.LC13, 8(%esp)
	movl	%ebx, 4(%esp)
	movl	%eax, (%esp)
	call	getopt
	cmpl	$-1, %eax
	je	.L126
	subl	$58, %eax
	cmpl	$58, %eax
	jbe	.L127
.L93:
	movl	log_stream, %edi
	movl	%edi, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	log_stream, %edx
	movl	$207, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edx, (%esp)
	call	__fprintf_chk
	movl	log_stream, %ecx
	movl	$.LC12, 8(%esp)
	movl	$1, 4(%esp)
	movl	%ecx, (%esp)
	call	__fprintf_chk
.L122:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	funlockfile
	movl	log_stream, %ebx
	movl	%ebx, (%esp)
	call	fflush
	movl	$1, (%esp)
	call	exit
	.p2align 4,,7
	.p2align 3
.L127:
	jmp	*.L102(,%eax,4)
	.section	.rodata
	.align 4
	.align 4
.L102:
	.long	.L94
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L95
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L96
	.long	.L93
	.long	.L93
	.long	.L97
	.long	.L93
	.long	.L93
	.long	.L98
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L93
	.long	.L99
	.long	.L93
	.long	.L100
	.long	.L101
	.text
	.p2align 4,,7
	.p2align 3
.L101:
	movl	optarg, %edx
	movl	%edx, (%esp)
	call	atoi
	movl	%eax, 68(%esp)
	jmp	.L120
	.p2align 4,,7
	.p2align 3
.L100:
	movl	optarg, %esi
	movl	%esi, (%esp)
	call	atoi
	movl	%eax, %esi
	jmp	.L120
	.p2align 4,,7
	.p2align 3
.L99:
	movl	optarg, %ecx
	movl	%ecx, (%esp)
	call	atoi
	movl	%eax, %edi
	jmp	.L120
	.p2align 4,,7
	.p2align 3
.L98:
	movl	$prog_name, 8(%esp)
	movl	$.LC9, 4(%esp)
	movl	$1, (%esp)
	call	__printf_chk
.L103:
	addl	$276, %esp
	xorl	%eax, %eax
	popl	%ebx
	popl	%esi
	popl	%edi
	movl	%ebp, %esp
	popl	%ebp
	ret
	.p2align 4,,7
	.p2align 3
.L97:
	movl	optarg, %edx
	movl	%edx, (%esp)
	call	atoi
	movl	%eax, 72(%esp)
	jmp	.L120
	.p2align 4,,7
	.p2align 3
.L96:
	movl	optarg, %eax
	movl	%eax, (%esp)
	call	atoi
	movl	%eax, 76(%esp)
	jmp	.L120
	.p2align 4,,7
	.p2align 3
.L95:
	movl	log_stream, %ecx
	movl	%ecx, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	$203, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%esi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	optopt, %edi
	movl	log_stream, %ebx
	movl	$.LC10, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edi, 12(%esp)
	movl	%ebx, (%esp)
	call	__fprintf_chk
	jmp	.L122
	.p2align 4,,7
	.p2align 3
.L94:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	log_stream, %edi
	movl	$205, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%esi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edi, (%esp)
	call	__fprintf_chk
	movl	optopt, %ebx
	movl	log_stream, %edx
	movl	$.LC11, 8(%esp)
	movl	$1, 4(%esp)
	movl	%ebx, 12(%esp)
	movl	%edx, (%esp)
	call	__fprintf_chk
	jmp	.L122
	.p2align 4,,7
	.p2align 3
.L126:
	movl	72(%esp), %ecx
	testl	%ecx, %ecx
	jle	.L128
	movl	76(%esp), %eax
	testl	%eax, %eax
	js	.L129
	leal	228(%esp), %ebx
	movl	%ebx, %eax
	call	utility_time_init
	movl	76(%esp), %eax
	movl	$1, %ecx
	movl	%ebx, (%esp)
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%eax, 56(%esp)
	movl	%edx, 60(%esp)
	call	to_utility_time
	leal	260(%esp), %edx
	movl	%ebx, %eax
	call	to_timespec
	testl	%edi, %edi
	jle	.L130
	cmpl	68(%esp), %edi
	jg	.L131
	cmpl	$-1, %esi
	setne	67(%esp)
	testl	%esi, %esi
	jle	.L132
.L109:
	leal	244(%esp), %ebx
	movl	%ebx, %eax
	call	utility_time_init
	movl	%esi, %edx
	movl	%esi, %eax
	sarl	$31, %edx
	movl	$1, %ecx
	movl	%ebx, (%esp)
	call	to_utility_time
	xorl	%edx, %edx
	movl	$2, %ecx
	movl	$100, %eax
	movl	$0, 268(%esp)
	call	to_utility_time_dyn
	movl	$1, %ecx
	movl	%eax, %esi
	movl	72(%esp), %eax
	movl	%eax, %edx
	sarl	$31, %edx
	movl	%edx, 52(%esp)
	movl	%eax, 48(%esp)
	call	to_utility_time_dyn
	leal	268(%esp), %ecx
	movl	%ecx, 16(%esp)
	movl	$10, 12(%esp)
	movl	%esi, 8(%esp)
	movl	$0, (%esp)
	movl	%eax, 4(%esp)
	call	create_cpu_busyloop
	cmpl	$-2, %eax
	je	.L133
	cmpl	$-4, %eax
	je	.L134
	testl	%eax, %eax
	.p2align 4,,5
	jne	.L135
	movl	68(%esp), %eax
	movl	$1, %ecx
	movl	%eax, %edx
	sarl	$31, %edx
	call	to_utility_time_dyn
	movl	%edi, %edx
	movl	$1, %ecx
	sarl	$31, %edx
	movl	%eax, %esi
	movl	%edi, %eax
	call	to_utility_time_dyn
	movl	$0, 8(%esp)
	movl	%esi, 4(%esp)
	movl	%eax, (%esp)
	call	sched_deadline_enter
	testl	%eax, %eax
	jne	.L136
	movl	$0, 4(%esp)
	movl	$jmp_env, (%esp)
	call	__sigsetjmp
	testl	%eax, %eax
	jne	.L114
	movl	76(%esp), %ecx
	leal	244(%esp), %edx
	cmpb	$0, 67(%esp)
	leal	260(%esp), %esi
	movl	$chunk_counter, 4(%esp)
	cmove	%eax, %edx
	testl	%ecx, %ecx
	movl	268(%esp), %ecx
	cmovne	%esi, %eax
	movl	$t1_abs, (%esp)
	call	hog_cpu
	movl	chunk_counter, %edx
	movl	$t1_abs, %eax
	call	print_stats
.L114:
	movl	268(%esp), %eax
	movl	%eax, (%esp)
	call	destroy_cpu_busyloop
	jmp	.L103
.L125:
	call	__errno_location
	movl	(%eax), %edi
	movl	%edi, (%esp)
	call	strerror
	movl	log_stream, %ecx
	movl	%ecx, (%esp)
	movl	%eax, %ebx
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	$145, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%esi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %edx
	movl	%ebx, 12(%esp)
	movl	$.LC8, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edx, (%esp)
	call	__fprintf_chk
	jmp	.L122
.L124:
	call	__errno_location
	movl	(%eax), %ecx
	movl	%ecx, (%esp)
	call	strerror
	movl	log_stream, %edx
	movl	%edx, (%esp)
	movl	%eax, %ebx
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	$139, 32(%esp)
	movl	$.LC2, 28(%esp)
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
.L121:
	movl	log_stream, %edi
	movl	$1, 4(%esp)
	movl	%edi, (%esp)
	call	__fprintf_chk
	jmp	.L122
.L123:
	call	__errno_location
	movl	(%eax), %edi
	movl	%edi, (%esp)
	call	strerror
	movl	log_stream, %ecx
	movl	%ecx, (%esp)
	movl	%eax, %ebx
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	log_stream, %edx
	movl	$139, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%esi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edx, (%esp)
	call	__fprintf_chk
	movl	%ebx, 12(%esp)
	movl	$.LC6, 8(%esp)
	jmp	.L121
.L128:
	movl	log_stream, %edi
	movl	%edi, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	log_stream, %edx
	movl	$212, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edx, (%esp)
	call	__fprintf_chk
	movl	log_stream, %esi
	movl	$.LC14, 8(%esp)
	movl	$1, 4(%esp)
	movl	%esi, (%esp)
	call	__fprintf_chk
	jmp	.L122
.L136:
	movl	log_stream, %edi
	movl	%edi, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	log_stream, %ecx
	movl	$258, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%ecx, (%esp)
	call	__fprintf_chk
	movl	log_stream, %edx
	movl	$.LC22, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edx, (%esp)
	call	__fprintf_chk
	jmp	.L122
.L135:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %edi
	call	getpid
	movl	log_stream, %ebx
	movl	$250, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%edi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%ebx, (%esp)
	call	__fprintf_chk
	movl	log_stream, %ecx
	movl	$.LC21, 8(%esp)
	movl	$1, 4(%esp)
	movl	%ecx, (%esp)
	call	__fprintf_chk
	jmp	.L122
.L134:
	movl	log_stream, %edx
	movl	%edx, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	$248, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%esi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	72(%esp), %edi
	movl	log_stream, %ebx
	movl	$.LC20, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edi, 12(%esp)
	movl	%ebx, (%esp)
	call	__fprintf_chk
	jmp	.L122
.L133:
	movl	log_stream, %ecx
	movl	%ecx, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	log_stream, %edx
	movl	$246, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%esi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edx, (%esp)
	call	__fprintf_chk
	movl	72(%esp), %eax
	movl	log_stream, %edi
	movl	$.LC19, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, 12(%esp)
	movl	%edi, (%esp)
	call	__fprintf_chk
	jmp	.L122
.L132:
	cmpb	$0, 67(%esp)
	je	.L109
	movl	log_stream, %edx
	movl	%edx, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	log_stream, %ecx
	movl	$229, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%ecx, (%esp)
	call	__fprintf_chk
	movl	log_stream, %esi
	movl	$.LC18, 8(%esp)
	movl	$1, 4(%esp)
	movl	%esi, (%esp)
	call	__fprintf_chk
	jmp	.L122
.L131:
	movl	log_stream, %eax
	movl	%eax, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %edi
	call	getpid
	movl	log_stream, %ebx
	movl	$226, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%edi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%ebx, (%esp)
	call	__fprintf_chk
	movl	log_stream, %edx
	movl	$.LC17, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edx, (%esp)
	call	__fprintf_chk
	jmp	.L122
.L130:
	movl	log_stream, %ecx
	movl	%ecx, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %esi
	call	getpid
	movl	$223, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%esi, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	log_stream, %eax
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%eax, (%esp)
	call	__fprintf_chk
	movl	log_stream, %edi
	movl	$.LC16, 8(%esp)
	movl	$1, 4(%esp)
	movl	%edi, (%esp)
	call	__fprintf_chk
	jmp	.L122
.L129:
	movl	log_stream, %edx
	movl	%edx, (%esp)
	call	flockfile
	call	pthread_self
	movl	%eax, %ebx
	call	getpid
	movl	log_stream, %esi
	movl	$215, 32(%esp)
	movl	$.LC2, 28(%esp)
	movl	$__FUNCTION__.6100, 24(%esp)
	movl	%ebx, 20(%esp)
	movl	$prog_name, 12(%esp)
	movl	%eax, 16(%esp)
	movl	$.LC5, 8(%esp)
	movl	$1, 4(%esp)
	movl	%esi, (%esp)
	call	__fprintf_chk
	movl	log_stream, %ecx
	movl	$.LC15, 8(%esp)
	movl	$1, 4(%esp)
	movl	%ecx, (%esp)
	call	__fprintf_chk
	jmp	.L122
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

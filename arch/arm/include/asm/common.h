
static inline unsigned long get_program_counter(void)
{
	unsigned long pc;

	__asm__ __volatile__(
                "mov    %0, pc\n"
                : "=r" (pc)
                :
                : "memory");

	return pc;
}


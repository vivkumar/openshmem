#ifndef _ATOMIC_H
#define _ATOMIC_H 1

extern void __shmem_atomic_init(void);
extern void __shmem_atomic_finalize(void);

extern void handler_swap_out();
extern void handler_swap_bak();

#endif /* _ATOMIC_H */

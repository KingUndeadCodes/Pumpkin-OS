#ifndef __SETJMP_H__
#define __SETJMP_H__

typedef struct { unsigned int regs[6]; } jmp_buf[1]; // ebx, esi, edi, ebp, esp, eip

#ifdef __cplusplus
extern "C" {
#endif

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#ifdef __cplusplus
}
#endif

#endif

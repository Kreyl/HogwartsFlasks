/*
 * syscalls.c
 *
 *  Created on: 13 апр. 2024 г.
 *      Author: layst
 */

#include <errno.h>
#include <sys/stat.h>

// See https://sourceware.org/newlib/libc.html#Stubs
//void* _sbrk(int incr) {
//    extern uint8_t __heap_base__;
//    extern uint8_t __heap_end__;
//
//    static uint8_t *current_end = &__heap_base__;
//    uint8_t *current_block_address = current_end;
//
//    incr = (incr + 3) & (~3);
//    if(current_end + incr > &__heap_end__) {
//        errno = ENOMEM;
//        return (void*) -1;
//    }
//    current_end += incr;
//    return (void*)current_block_address;
//}

int _write(int file, char *ptr, int len) { // XXX Make it good
//    int count = len;
//    if (file == 1) { // stdout
//        while (count > 0) {
//            while(!(USART1->ISR & USART_ISR_TXE));
//            USART1->TDR = *ptr;
//            ++ptr;
//            --count;
//        }
//    }
    return len;
}

int _close(int file) {
    return -1;
}

int _fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int _getpid(void) {
     return 1;
}

int _isatty(int file) {
     return 1;
}

#undef errno
extern int errno;
int _kill(int pid, int sig) {
    errno = EINVAL;
    return -1;
}

int _lseek(int file, int ptr, int dir) {
    return 0;
}

int _read(int file, char *ptr, int len) {
    return 0;
}

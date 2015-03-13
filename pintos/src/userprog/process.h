#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name, struct thread* parent);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void exec_process(char* filename);

#endif /* userprog/process.h */

#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "userprog/process.h"

struct lock filelock;

static void syscall_handler (struct intr_frame *);
static void sys_write(struct intr_frame *);
static void sys_exit(int);
static void sys_exec(struct intr_frame *);

static void sys_exec(struct intr_frame *f)
{
  thread_current()->gave_birth = 0;
  int pid = process_execute((char*)*((void**)(f->esp + 4)), thread_current());
  /*char* name = (char*)*((void**)(f->esp + 4));
  int i = 0;
  for(i = 0; i < 16 && *name != '\0' && name != ' '; i++)
  {
    thread_current()->name[i] = name;
  }*/
  while(thread_current()->gave_birth != 1)
    {
      //Spin lock until done exec'ing
    }
  thread_current()->gave_birth = 0;
  if(pid < 0)
    {
      f->eax = -1;
      return;
    }
  f->eax = pid;
}

static void sys_exit(int exit_code)
{   
  thread_current()->child_ret = exit_code;
  if(thread_current()->parent != NULL)
    {
      thread_current()->parent->child_ret = exit_code;
    }
  thread_exit();
}

static void sys_wait(struct intr_frame *f)
{
  tid_t child_tid = *(int*)((void*)(f->esp + 4));
  if(!tid_exists(child_tid))
    {
      f->eax = -1;
      return;
    }
  else
    {
      process_wait(child_tid);
    }
  process_wait(child_tid);
  f->eax = thread_current()->child_ret;
  thread_current()->child_ret = 0;
}

void
syscall_init (void) 
{
    lock_init(&filelock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void**
args_checker(int args, struct intr_frame *f)
{
  int i = 0;
  void* user_pointer = f->esp;
  for(i = 0; i<args; i++)
  {
    user_pointer += 4;
    if(!valid_user_pointer(thread_current()->pagedir, user_pointer))
      {
	sys_exit(-1);
      }
  }
  return f->esp;
}

static void
sys_write(struct intr_frame *f)
{
  unsigned fd = *(int*)(f->esp + 4);
  char* message = (char*)(*(void**)(f->esp + 8));
  unsigned int length = *(int*)(f->esp + 12);
  args_checker(3,f);
  if (!is_user_vaddr(message) || message < 0x08048000)
      sys_exit(-1);
  void *ptr = pagedir_get_page(thread_current()->pagedir, message);
  if(message == NULL)
      sys_exit(-1);
  if (!ptr)
  {
      sys_exit(-1);
  }
  if(fd == 0)
  {
    sys_exit(-1);
    //printf("Segmentation fault: cannot write to console input.");
  }
  else if(fd == 1)
  {
    int remaining = (int)length;
    char* buffer = message;
    while (remaining >= 1023)
    {
      putbuf(buffer, 1023);
      remaining -= 1024;
      buffer += 1024;
    }
    putbuf(buffer, remaining);
  }
  else
  {
      //check for fdtables
      if(fd >= thread_current()->current_fd)
      {
	  f->eax=-1;
	  sys_exit(-1);
      }
      else
      {
	  lock_acquire(&filelock);
	  f->eax=file_write(thread_current()->fd_table[fd], message, (off_t)length);
	  lock_release(&filelock);
      }
  }
}

static void
sys_read(struct intr_frame * f)
{
  unsigned fd = *(int*)(f->esp + 4);
  char* message = (char*)(*(void**)(f->esp + 8));
  unsigned length = *(unsigned*)(f->esp + 12);
  
  if (!is_user_vaddr(message) || message < 0x08048000)
      sys_exit(-1);
  void *ptr = pagedir_get_page(thread_current()->pagedir, message);
  if(message == NULL)
      sys_exit(-1);
  if (!ptr)
  {
      sys_exit(-1);
  }
  //message = ptr;
  int i;
/*  
  char * local = (char*) message;
  for(i = 0; i < length; i++)
  {
      if (!is_user_vaddr(local) || local < 0x08048000)
	  sys_exit(-1);      
      local++;
      }*/
  if(fd == 0)
  {
      int i;
      uint8_t* local_buffer = (uint8_t *) message;
      for(i = 0; i < length; i++)
      {
	  message[i] = input_getc();
      }
      f->eax = length;
  }
  else if(fd == 1)
  {
      
  }
  else
  {
      if(fd >= thread_current()->current_fd)
      {
	  //f->eax=-1;
	  sys_exit(-1);
      }
      else
      {
	  lock_acquire(&filelock);
	  f->eax = file_read(thread_current()->fd_table[fd],message,length);
	  lock_release(&filelock);
      }      
  }

}

static void
syscall_handler (struct intr_frame *f) 
{
  //printf ("system call!\n");

  int *syscall_num = f->esp;
  
  if(pagedir_get_page(thread_current()->pagedir, f->esp) == NULL)
    {
      thread_current()->parent->child_ret = -1;
      thread_current()->child_ret = -1;
      f->esp = -1;
      sys_exit(-1);
      return;
    }
  //check if esp is right
  if(f->esp < 0x08048000 || f->esp > PHYS_BASE)
    { sys_exit(-1);}
  
  switch(*syscall_num)
    {
    case SYS_HALT:
      {
	  shutdown_power_off();
	break;
      }
	  case SYS_CREATE:
    {

		char *filename =  (char*)(*(void**)(f->esp + 4));
		int filesize = *(int*)(f->esp+8);
		//printf(filename);
		if(!valid_user_pointer(thread_current()->pagedir, filename))
		{
			sys_exit(-1);
		}
		bool success = filesys_create(filename, filesize);
		f->eax = success;
		break;
    }
    case SYS_OPEN:
    {
		char *filename =  (char*)(*(void**)(f->esp + 4));
		if(!valid_user_pointer(thread_current()->pagedir, filename))
		{
			sys_exit(-1);
		}
		struct file *fil = filesys_open(filename);
		if(!fil)
		{
			//failed open
			f->eax = -1;
		}
		else
		{
			thread_current()->fd_table[thread_current()->current_fd] = fil;
			int ret = thread_current()->current_fd;
			thread_current()->current_fd++;
			f->eax = ret;
		}
		break;
    }
    case SYS_CLOSE:
    {
		int fd = *(int*)(f->esp+4);
		if(fd >= thread_current()->current_fd)
		{
			sys_exit(-1);
		}
		if(thread_current()->fd_table[fd] == NULL)
			sys_exit(-1);
		struct file* fil = thread_current()->fd_table[fd];
		//thread_current()->current_fd--;
		thread_current()->fd_table[fd] = NULL;
		
		file_close(fil);
		break;
    }
    case SYS_READ:
    {
		sys_read(f);	
		//f->eax = 239;
		break;
    }
    case SYS_EXIT:
    {
		args_checker(1, f);
		sys_exit(*(int*)(f->esp + 4));
		break;
    }
    case SYS_EXEC:
    {
		args_checker(1, f);
		sys_exec(f);
		break;
    }
    case SYS_WAIT:
    {
		sys_wait(f);
		break;
    }
    case SYS_REMOVE:
    {
	char* message = (char*)(*(void**)(f->esp + 4));
	bool ret = filesys_remove(message);
	f->eax = ret;
	break;
    }
    case SYS_FILESIZE:
    {
		int fd = *(int*)(f->esp+4);
		struct file* fil = thread_current()->fd_table[fd];
		f->eax = file_length(fil);
		break;
    }
    case SYS_SEEK:
    {
		int fd = *(int*)(f->esp + 4);
		unsigned position = *(unsigned*)(f->esp+8);
		if(fd >= thread_current()->current_fd)
		{
			f->eax = -1;
			sys_exit(-1);
		}
		struct file* fil = thread_current()->fd_table[fd];
		file_seek(fil,position);
		break;
    }
    case SYS_WRITE:
    {
		args_checker(3, f);
       	sys_write(f);
		break;
    }
    case SYS_TELL:
    {
	int fd = *(int*)(f->esp+4);
	struct file *fil = thread_current()->fd_table[fd];
	f->eax = file_tell(fil);
	break;
    }
	default:
    {
		sys_exit(-1);
    }
    }
  //thread_exit ();
}


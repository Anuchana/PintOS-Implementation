#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

static void validate_user_pointer(const void *vaddr);
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
validate_user_pointer(const void *uaddr) {
  if(uaddr==NULL || !is_user_vaddr(uaddr) || pagedir_get_page(thread_current()->pagedir, uaddr) == NULL) {
    thread_exit();
  }
}

static void
syscall_handler (struct intr_frame *f ) 
{
  int syscall_number;
  validate_user_pointer(f->esp);
  syscall_number = *(int *)(f->esp);

  switch (syscall_number)
  {
  case SYS_HALT:
    shutdown_power_off();
    break;
  
  default:
    break;
  }
}

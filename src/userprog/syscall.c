#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/off_t.h"
#include "stdbool.h"
#include "stdint.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "threads/thread.h"
#include "threads/malloc.h"

#define MAX_SYSCALL (SYS_INUMBER)

typedef void (*syscall_func) (struct intr_frame *);

static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *uaddr);
static void * check_ptr (const void *vaddr);
static void exit_error (void);
static struct thread_file * find_file (int fd);
static bool is_valid_p (void *esp, uint8_t argc);
static syscall_func syscall_table[MAX_SYSCALL + 1];

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  syscall_table[SYS_HALT] = syscall_halt;
  syscall_table[SYS_EXIT] = syscall_exit;
  syscall_table[SYS_EXEC] = syscall_exec;
  syscall_table[SYS_WAIT] = syscall_wait;
  syscall_table[SYS_CREATE] = syscall_create;
  syscall_table[SYS_REMOVE] = syscall_remove;
  syscall_table[SYS_OPEN] = syscall_open;
  syscall_table[SYS_FILESIZE] = syscall_filesize;
  syscall_table[SYS_READ] = syscall_read;
  syscall_table[SYS_WRITE] = syscall_write;
  syscall_table[SYS_SEEK] = syscall_seek;
  syscall_table[SYS_TELL] = syscall_tell;
  syscall_table[SYS_CLOSE] = syscall_close;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_ptr ((int *)f->esp + 1);
  int syscall_num = *(int *) f->esp;
  if (syscall_num < 0 || syscall_num > MAX_SYSCALL)
    exit_error ();
  syscall_table[syscall_num] (f);
}

static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
  return result;
}

static void
exit_error (void)
{
  thread_current ()->exit_status = -1;
  thread_exit ();
}

static void *
check_ptr (const void *vaddr)
{
  if (!is_user_vaddr (vaddr))
    exit_error ();
  void *ptr = pagedir_get_page (thread_current ()->pagedir, vaddr);
  if (!ptr)
    exit_error ();
  uint8_t *uaddr = (uint8_t *) vaddr;
  for (; uaddr < (uint8_t *) vaddr + sizeof (int); uaddr++)
    if (get_user (uaddr) == -1)
      exit_error ();
  return ptr;
}

static bool
is_valid_p (void *esp, uint8_t argc)
{
  uint8_t i;
  for (i = 0; i < argc; i++)
    if (!is_user_vaddr(esp) || !pagedir_get_page(thread_current ()->pagedir, esp))
      return false;
  return true;
}

void
syscall_halt (struct intr_frame *f UNUSED)
{
  shutdown_power_off ();
}

void
syscall_exit (struct intr_frame *f)
{
  check_ptr ((uint32_t *)f->esp + 1);
  int status = *(int *)((uint32_t *)f->esp + 1);
  thread_current ()->exit_status = status;
  thread_exit ();
}

void
syscall_exec (struct intr_frame *f)
{
  check_ptr ((int *)f->esp + 1);
  char *cmd_line = *(char **)((uint32_t *)f->esp + 1);
  check_ptr (cmd_line);
  f->eax = process_execute (cmd_line);
}

void
syscall_wait (struct intr_frame *f)
{
  check_ptr ((int *)f->esp + 1);
  tid_t tid = *(tid_t *) (f->esp + 4);
  f->eax = process_wait (tid);
}

void
syscall_create (struct intr_frame *f)
{ 
  check_ptr((uint32_t *)f->esp + 5);
  check_ptr((void *)*((uint32_t *)f->esp + 4));
  const char *file = (const char *)*((uint32_t *)f->esp + 1);
  int initial_size = *((uint32_t *)f->esp + 2);
  acquire_f ();
  f->eax = filesys_create (file, initial_size);
  release_f ();
}

void
syscall_remove (struct intr_frame *f)
{
  check_ptr ((uint32_t *)f->esp + 1);
  check_ptr ((void *)*((uint32_t *)f->esp + 1));
  acquire_f ();
  f->eax = filesys_remove ((const char *)*((uint32_t *)f->esp + 1));
  release_f ();
}

void
syscall_open (struct intr_frame *f)
{
  check_ptr ((uint32_t *)f->esp + 1);
  check_ptr ((void *)*((uint32_t *)f->esp + 1));
  acquire_f ();
  struct file * file_opened = filesys_open((const char *)*((uint32_t *)f->esp + 1));
  release_f ();
  struct thread * t = thread_current();
  if (file_opened)
  {
    struct thread_file *thread_file_temp = malloc(sizeof(struct thread_file));
    thread_file_temp->fd = t->max_fd++;
    thread_file_temp->file = file_opened;
    list_push_back (&t->files, &thread_file_temp->elem);
    f->eax = thread_file_temp->fd;
  } 
  else
    f->eax = -1;
}

void
syscall_filesize (struct intr_frame *f)
{
  check_ptr ((uint32_t *)f->esp + 1);
  struct thread_file * thread_file_temp = find_file (*((uint32_t *)f->esp + 1));
  if (thread_file_temp)
  {
    acquire_f ();
    f->eax = file_length (thread_file_temp->file);
    release_f ();
  } 
  else
    f->eax = -1;
}

void
syscall_read (struct intr_frame *f)
{
  int fd = *((uint32_t *)f->esp + 1);
  uint8_t * buffer = (uint8_t*)*((uint32_t *)f->esp + 2);
  off_t size = *((uint32_t *)f->esp + 3);
  if (!is_valid_p (buffer, 1) || !is_valid_p (buffer + size, 1)){
    exit_error ();
  }
  if (fd == 0)
  {
    for (int i = 0; i < size; i++)
      buffer[i] = input_getc();
    f->eax = size;
  }
  else
  {
    struct thread_file * thread_file_temp = find_file (fd);
    if (thread_file_temp)
    {
      acquire_f ();
      f->eax = file_read (thread_file_temp->file, buffer, size);
      release_f ();
    } 
    else
      f->eax = -1;
  }
}

void
syscall_write (struct intr_frame *f)
{
  check_ptr ((uint32_t *)f->esp + 7);
  check_ptr ((void *)*((uint32_t *)f->esp + 6));
  int fd = *((uint32_t *) f->esp + 1);
  const char *buffer = (const char *)*((uint32_t *)f->esp + 2);
  off_t size = *((uint32_t *)f->esp + 3);
  if (fd == 1) 
  {
    putbuf (buffer,size);
    f->eax = size;
  }
  else
  {
    struct thread_file * thread_file_temp = find_file (fd);
    if (thread_file_temp)
    {
      acquire_f ();
      f->eax = file_write (thread_file_temp->file, buffer, size);
      release_f ();
    } 
    else
      f->eax = 0;
  }
}

void
syscall_seek (struct intr_frame *f)
{
  check_ptr ((uint32_t *)f->esp + 5);
  struct thread_file *file_temp = find_file (*((uint32_t *)f->esp + 1));
  if (file_temp)
  {
    acquire_f ();
    file_seek (file_temp->file, *((uint32_t *)f->esp + 2));
    release_f ();
  }
}

void
syscall_tell (struct intr_frame *f)
{
  check_ptr ((uint32_t *)f->esp + 1);
  struct thread_file *thread_file_temp = find_file (*((uint32_t *)f->esp + 1));
  if (thread_file_temp)
  {
    acquire_f ();
    f->eax = file_tell (thread_file_temp->file);
    release_f ();
  }
  else
    f->eax = -1;
}

void
syscall_close (struct intr_frame *f)
{
  check_ptr ((uint32_t *)f->esp + 1);
  struct thread_file * opened_file = find_file (*((uint32_t *)f->esp + 1));
  if (opened_file)
  {
    acquire_f ();
    file_close (opened_file->file);
    release_f ();
    list_remove (&opened_file->elem);
    free (opened_file);
  }
}

static struct thread_file * 
find_file (int fd)
{
  struct list_elem *e;
  struct thread_file * thread_file_temp = NULL;
  struct list *files = &thread_current ()->files;
  for (e = list_begin (files); e != list_end (files); e = list_next (e))
  {
    thread_file_temp = list_entry (e, struct thread_file, elem);
    if (fd == thread_file_temp->fd)
      return thread_file_temp;
  }
  return false;
}

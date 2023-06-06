/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <p_stats.h>

#include <errno.h>

#define LECTURA 0
#define ESCRIPTURA 1

void *get_ebp();

int check_fd(int fd, int permissions)
{
  if (fd != 1)
    return -EBADF;
  if (permissions != ESCRIPTURA)
    return -EACCES;
  return 0;
}

void user_to_system(void)
{
  update_stats(&(current()->p_stats.user_ticks), &(current()->p_stats.elapsed_total_ticks));
}

void system_to_user(void)
{
  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
}

int sys_ni_syscall()
{
  return -ENOSYS;
}

int sys_getpid()
{
  return current()->PID;
}

int global_PID = 1000;

int ret_from_fork()
{
  return 0;
}

int sys_fork(void)
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;

  /* Any free task_struct? */
  if (list_empty(&freequeue))
    return -ENOMEM;

  lhcurrent = list_first(&freequeue);

  list_del(lhcurrent);

  uchild = (union task_union *)list_head_to_task_struct(lhcurrent);

  /* Copy the parent's task struct to child's */
  copy_data(current(), uchild, sizeof(union task_union));

  /* new pages dir */
  allocate_DIR((struct task_struct *)uchild);



  /* Allocate pages for DATA+STACK */
  int new_ph_pag, pag, i;
  page_table_entry *process_PT = get_PT(&uchild->task);
  

  /* Copy parent's SYSTEM and CODE to child. */
  
  page_table_entry *parent_PT = get_PT(current());

  for (pag = 0; pag < NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag = 0; pag < NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE + pag, get_frame(parent_PT, PAG_LOG_INIT_CODE + pag));
  }

  /* Copy data and stack pages */
  // And set read only
  for (pag = 0; pag <NUM_PAG_DATA; pag++)
  {
    set_ss_pag(process_PT, pag+PAG_LOG_INIT_DATA, get_frame(parent_PT, pag+PAG_LOG_INIT_DATA));
    int frame = get_frame(parent_PT, pag+PAG_LOG_INIT_DATA);
    phys_mem[frame]++;
    parent_PT[pag+PAG_LOG_INIT_DATA].bits.rw = 0;
    process_PT[pag+PAG_LOG_INIT_DATA].bits.rw = 0;
  }

  /* Copy shared memory pages*/

  for (pag = NUM_PAG_KERNEL + NUM_PAG_CODE + NUM_PAG_DATA; pag < TOTAL_PAGES; pag++)
  {
    if (get_frame(parent_PT, pag)!=0){
    int shared = get_frame(parent_PT, pag);
    int i;
    for (i = 0; i < NUM_SHRD_PAGES; i++)
    {
      if (shared_pages[i].frame_id == shared)
      {
        ++shared_pages[i].num_refs;
        set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
      }
    }
  }
  }

  /* Deny access to the child's memory space */
  set_cr3(get_DIR(current()));

  uchild->task.PID = ++global_PID;
  uchild->task.state = ST_READY;

  int register_ebp; /* frame pointer */
  /* Map Parent's ebp to child's stack */
  register_ebp = (int)get_ebp();
  register_ebp = (register_ebp - (int)current()) + (int)(uchild);

  uchild->task.register_esp = register_ebp + sizeof(DWord);

  DWord temp_ebp = *(DWord *)register_ebp;
  /* Prepare child stack for context switch */
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord *)(uchild->task.register_esp) = (DWord)&ret_from_fork;
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord *)(uchild->task.register_esp) = temp_ebp;

  /* Set stats to 0 */
  init_stats(&(uchild->task.p_stats));

  /* Queue child process into readyqueue */
  uchild->task.state = ST_READY;
  list_add_tail(&(uchild->task.list), &readyqueue);

  return uchild->task.PID;
}

#define TAM_BUFFER 512

int sys_write(int fd, char *buffer, int nbytes)
{
  char localbuffer[TAM_BUFFER];
  int bytes_left;
  int ret;

  if ((ret = check_fd(fd, ESCRIPTURA)))
    return ret;
  if (nbytes < 0)
    return -EINVAL;
  if (!access_ok(VERIFY_READ, buffer, nbytes))
    return -EFAULT;

  bytes_left = nbytes;
  while (bytes_left > TAM_BUFFER)
  {
    copy_from_user(buffer, localbuffer, TAM_BUFFER);
    ret = sys_write_console(localbuffer, TAM_BUFFER);
    bytes_left -= ret;
    buffer += ret;
  }
  if (bytes_left > 0)
  {
    copy_from_user(buffer, localbuffer, bytes_left);
    ret = sys_write_console(localbuffer, bytes_left);
    bytes_left -= ret;
  }
  return (nbytes - bytes_left);
}

extern int zeos_ticks;

int sys_gettime()
{
  return zeos_ticks;
}

void sys_exit()
{
  int i;

  page_table_entry *process_PT = get_PT(current());

  // Deallocate all the propietary physical pages
  for (i = 0; i < NUM_PAG_DATA; i++)
  {
    int frame = get_frame(process_PT, PAG_LOG_INIT_DATA + i);
    free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA + i));
    del_ss_pag(process_PT, PAG_LOG_INIT_DATA + i);
  }
  // Deallocate all the shared pages
  for (int pag = NUM_PAG_KERNEL + NUM_PAG_CODE + NUM_PAG_DATA; pag < TOTAL_PAGES; pag++)
  {

    int shared = get_frame(process_PT, pag);
    int i;
    for (i = 0; i < NUM_SHRD_PAGES; i++)
    {
      if (shared_pages[i].frame_id == shared)
      {
        if (shared_pages[i].num_refs == 0 && shared_pages[i].marked == 1){
            // set page to 0
            unsigned int addr = pag << 12;
            memset((void *)addr, 0, PAGE_SIZE);
            shared_pages[i].marked = 0;
        }
              
        --shared_pages[i].num_refs;
        free_frame(get_frame(process_PT, pag));
        del_ss_pag(process_PT, pag);
        set_cr3(get_DIR(current()));
      }
    }
  }

  /* Free task_struct */
  list_add_tail(&(current()->list), &freequeue);

  current()->PID = -1;

  /* Restarts execution of the next process */
  sched_next_rr();
}

/* System call to force a task switch */
int sys_yield()
{
  force_task_switch();
  return 0;
}

extern int remaining_quantum;

int sys_get_stats(int pid, struct stats *st)
{
  int i;

  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats)))
    return -EFAULT;

  if (pid < 0)
    return -EINVAL;
  for (i = 0; i < NR_TASKS; i++)
  {
    if (task[i].task.PID == pid)
    {
      task[i].task.p_stats.remaining_ticks = remaining_quantum;
      copy_to_user(&(task[i].task.p_stats), st, sizeof(struct stats));
      return 0;
    }
  }
  return -ESRCH; /*ESRCH */
}

int sys_read(char *b, int maxchars)
{
  if (!access_ok(VERIFY_WRITE, b, sizeof(char *)))
    return -EFAULT;
  return read_circular_buffer(b, maxchars);
}

int sys_gotoxy(int x, int y)
{
  return change_xy(x, y);
}

int sys_set_color(int fg, int bg)
{
  return change_color(fg, bg);
}

/*función para encontrar logical_page libre*/
unsigned int free_addr()
{
  int found = 0;
  unsigned int free_page;
  for (int i = NUM_PAG_KERNEL + NUM_PAG_CODE + NUM_PAG_DATA; i < (TOTAL_PAGES) && !found; i++)
  {
    // Si get_frame==0, la página está libre
    if (!get_frame(get_PT(current()), i))
    {
      free_page = i;
      found = 1;
    }
  }
  if (found)
  {
    // unsigned int free_addr = free_page << 12;
    return free_page;
  }
  else
    return -ENOMEM;
}

void *sys_shmat(int id, void *addr)
{
  if (id < 0 || id > 9)
    return -EINVAL;
  if ((unsigned long)addr % PAGE_SIZE != 0)
    return -EINVAL;
  //Mirar si la addr es < TOTAL_PAGES
  if ((unsigned long)addr >> 12 > TOTAL_PAGES)
    return -EINVAL;

  page_table_entry *current_PT = get_PT(current());
  unsigned id_frame = shared_pages[id].frame_id;

  // If addr == null, find empty address
  if (addr == NULL)
  {
    unsigned int free_page = free_addr();
    if (free_page == -ENOMEM)
      return -ENOMEM;
    set_ss_pag(current_PT, free_page, id_frame);
    unsigned int free_adr = free_page << 12;
    ++shared_pages[id].num_refs;
    return (void *)free_adr;
  }
  else
  {
    // Check if addr is free if not, find empty address
    unsigned page = (unsigned long)addr >> 12;
    if (get_frame(current_PT, page) != 0)
    {
      unsigned int free_page = free_addr();
      if (free_page == -ENOMEM)
        return -ENOMEM;
      set_ss_pag(current_PT, free_page, id_frame);
      unsigned int free_adr = free_page << 12;
      ++shared_pages[id].num_refs;
      return (void *)free_adr;
    }
    else
    {
      set_ss_pag(current_PT, page, id_frame);
      ++shared_pages[id].num_refs;
      return addr;
    }
  }
}

int sys_shmdt(void *addr)
{
  if ((unsigned long)addr % PAGE_SIZE != 0)
    return -EINVAL;
  if ((unsigned long)addr >> 12 > TOTAL_PAGES)
    return -EINVAL;
  page_table_entry *current_PT = get_PT(current());
  unsigned page = (unsigned long)addr >> 12;
  if (get_frame(current_PT, page) <= NUM_PAG_KERNEL + NUM_PAG_CODE + NUM_PAG_DATA)
    return -EINVAL;
  else
  {
    int shared = get_frame(current_PT, page);
    if (shared == 0)
      return -EINVAL;
    // Encuentra el lugar en el array de shared_pages
    int i;
    for (i = 0; i < NUM_SHRD_PAGES; i++)
    {
      if (shared_pages[i].frame_id == shared)
      {
        --shared_pages[i].num_refs;
        break;
      }
    }

    if (shared_pages[i].num_refs == 0 && shared_pages[i].marked == 1)
    {
      // set page to 0
      memset((void *)addr, 0, PAGE_SIZE);
      shared_pages[i].marked = 0;
    }
    free_frame(get_frame(current_PT, page));
    del_ss_pag(current_PT, page);
    set_cr3(get_DIR(current()));
    return shared_pages[i].num_refs;
  }
}

int sys_shmrm(int id)
{
  if (id < 0 || id > 9)
    return -EINVAL;
  shared_pages[id].marked = 1;
    return 0;
}
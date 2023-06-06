/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>
#include <mm.h>
#include <utils.h>
#include <errno.h>

#include <sched.h>

#include <zeos_interrupt.h>

Gate idt[IDT_ENTRIES];
Register    idtR;

char char_map[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','�','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','�',
  '\0','�','\0','�','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

int zeos_ticks = 0;

void clock_routine()
{
  zeos_show_clock();
  zeos_ticks ++;
  
  schedule();
}

void keyboard_routine()
{
  unsigned char c = inb(0x60);
  
 // if (c&0x80) printc_xy(0, 0, char_map[c&0x7f]);
    if (c&0x80) write_circular_buffer(char_map[c&0x7f]);

}

void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
  /* ***************************                                         */
  /* flags = x xx 0x110 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
  /* ********************                                                */
  /* flags = x xx 0x111 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);

  //flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
  /* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
     the system calls will be thread-safe. */
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void clock_handler();
void keyboard_handler();
void system_call_handler();
void page_fault_handler2();

void setMSR(unsigned long msr_number, unsigned long high, unsigned long low);

void setSysenter()
{
  setMSR(0x174, 0, __KERNEL_CS);
  setMSR(0x175, 0, INITIAL_ESP);
  setMSR(0x176, 0, (unsigned long)system_call_handler);
}

void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  setInterruptHandler(32, clock_handler, 0);
  setInterruptHandler(33, keyboard_handler, 0);
  setInterruptHandler(14, page_fault_handler2, 0);

  setSysenter();

  set_idt_reg(&idtR);
}

void int_to_hex(unsigned int num, char *hex_string) {
    const char *hex_digits = "0123456789abcdef";
    int i = 0;

    while (num > 0) {
        int digit = num % 16;
        hex_string[i++] = hex_digits[digit];
        num /= 16;
    }
    while (i < sizeof(int) * 2) {
        hex_string[i++] = '0';
    }
    int len = i;
    for (i = 0; i < len / 2; i++) {
        char tmp = hex_string[i];
        hex_string[i] = hex_string[len - i - 1];
        hex_string[len - i - 1] = tmp;
    }
    hex_string[len] = '\0';
}

void page_fault_routine2(unsigned int error_code, unsigned int eip, unsigned int cr2)
{
  //Get the addr where the page fault happened
  unsigned int fault_addr = cr2;
  //Get the page
  unsigned int page = fault_addr >> 12;
  //If the page is not a data page, its page fault
  page_table_entry *current_pt = get_PT(current());
  unsigned int frame = get_frame(current_pt, page);
  
  //If the page is not a data page, its page fault
  if (page < NUM_PAG_KERNEL+NUM_PAG_CODE || page >= NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA){
    char buff[8];
    int_to_hex(eip, buff);
    printk("Process generates a PAGE FAULT expection at EIP: 0x");
    printk(buff);
    itoa(error_code, buff);
    printk("\nError: ");
    printk(buff);
    while(1);
  }
  else{
    //If the page is a data page, copy on write
    if (phys_mem[frame]==1){ //If is 1, change a rw
      current_pt[page].bits.rw = 1;
      set_cr3(get_DIR(current()));
    }
    else {
      //If is >1, copy the page
      int new_frame = alloc_frame();
      int temporal_page = TOTAL_PAGES-1;
      int temporal_frame = get_frame(current_pt, temporal_page);
    if (new_frame == -1) return -ENOMEM;
      if (temporal_frame !=0) { //If the temporal page is used, we desalocate it
        del_ss_pag(current_pt, temporal_page);
      }
      --phys_mem[frame];
      set_ss_pag(current_pt, temporal_page, new_frame);
      set_cr3(get_DIR(current()));
      copy_data((void*)((page)<<12), (void*)(temporal_page<<12), PAGE_SIZE);

      del_ss_pag(current_pt, temporal_page);
      del_ss_pag(current_pt, page);

      set_ss_pag(current_pt, page, new_frame);

      if (temporal_frame !=0) { //If the temporal page was used, we realocate it
        set_ss_pag(current_pt, temporal_page, temporal_frame);
      }
      set_cr3(get_DIR(current()));
      
    }
  }
  return;
  //Si todo va bien, se vuelve a ejecutar la instruccion que fallo
}


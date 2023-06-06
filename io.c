/*
 * io.c - 
 */

#include <io.h>
#include <errno.h>

#include <types.h>

/**************/
/** Screen  ***/
/**************/

#define NUM_COLUMNS 80
#define NUM_ROWS    25

Byte x, y=19;
char fg_color = 0x02;
char bg_color = 0x00;

/* Read a byte from 'port' */
Byte inb (unsigned short port)
{
  Byte v;

  __asm__ __volatile__ ("inb %w1,%0":"=a" (v):"Nd" (port));
  return v;
}

void printc(char c)
{
     __asm__ __volatile__ ( "movb %0, %%al; outb $0xe9" ::"a"(c)); /* Magic BOCHS debug: writes 'c' to port 0xe9 */
  if (c=='\n')
  {
    x = 0;
    y=(y+1)%NUM_ROWS;
  }
  else
  {
Word ch = (Word)(c & 0x00FF) | ((bg_color << 12) | (fg_color << 8));
	Word *screen = (Word *)0xb8000;
	screen[(y * NUM_COLUMNS + x)] = ch;
    if (++x >= NUM_COLUMNS)
    {
      x = 0;
      y=(y+1)%NUM_ROWS;
    }
  }
}

void printc_xy(Byte mx, Byte my, char c)
{
  Byte cx, cy;
  cx=x;
  cy=y;
  x=mx;
  y=my;
  printc(c);
  x=cx;
  y=cy;
}

void printk(char *string)
{
  int i;
  for (i = 0; string[i]; i++)
    printc(string[i]);
}

#define BUFFER_SIZE 32

char buffer_circular[BUFFER_SIZE];
int head = 0;
int tail = 0;
int count = 0; // Numero de elementos en el buffer


/* Funcion que escribe en el buffer circular */
void write_circular_buffer(char c){
  buffer_circular[head] = c;
  head = (head + 1) % BUFFER_SIZE;
  if (count == BUFFER_SIZE){
    tail = (tail + 1) % BUFFER_SIZE;
  }
  else{
    count++;
  }
}

/* Funcion que lee del buffer circular */
int read_circular_buffer(char *c, int size){
  if (count == 0){
    return 0;
  }
  int i=0;
  while (i < size && count > 0){
    c[i] = buffer_circular[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    count--;
    i++;
  }
  return i;
}

int change_xy(int new_x, int new_y){
  if (new_x >= 0 && new_x < NUM_COLUMNS && new_y >= 0 && new_y < NUM_ROWS){
  //Se lo asignamos a los bytes x e y
  x = (Byte)(new_x & 0x00FF);
  y = (Byte)(new_y & 0x00FF);
  return 0;
  }
  else{
    return -EINVAL;
  }
}

int change_color(int new_fg, int new_bg){
  if (new_fg >= 0 && new_fg < 16 && new_bg >= 0 && new_bg < 16){
    fg_color = (char)(new_fg);
    bg_color = (char)(new_bg);
    return 0;
  }
  else{
    return -EINVAL;
  }

}

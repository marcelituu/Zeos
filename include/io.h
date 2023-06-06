/*
 * io.h - Definició de l'entrada/sortida per pantalla en mode sistema
 */

#ifndef __IO_H__
#define __IO_H__

#include <types.h>

/** Screen functions **/
/**********************/

Byte inb (unsigned short port);
void printc(char c);
void printc_xy(Byte x, Byte y, char c);
void printk(char *string);
void write_circular_buffer(char c);
int read_circular_buffer(char *c, int size);
int change_xy(int new_x, int new_y);
int change_color(int new_fg, int new_bg);


#endif  /* __IO_H__ */

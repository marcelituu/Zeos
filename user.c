#include <libc.h>
#include <stddef.h>
char buff[24];
int frames=0;
int pid;

float get_fps(){
  float ticks = gettime()/18.0;
  float fps = (float)frames/ticks;
  return fps;
}


int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

  //Test milestone 1
  
  gotoxy(80,90);
  set_color(1,0);
 /*while(1){
    char buff[4];
    int c=read(buff,4);
    if (c!=-1) write(1,buff,c);
    }
 */ 
  //Test shmat
  int id = 7;
  //addr de la pagina 340
  unsigned int addr = 340 << 12;
  unsigned int addr1 = 350 << 12;
  unsigned int addr2 = NULL;
  void* mod;
  void* mod1;

  mod = shmat(7, addr2);
  int* ptr = (int*) mod;
  *ptr = 7;
  itoa (*ptr, buff);
  write(1, buff, strlen(buff));

  char *buff2;
  buff2 = "\nMiramos pagina donde se mapea\n";
  write(1, buff2, strlen(buff2));

  unsigned int pa=(unsigned long)mod >> 12;
  itoa (pa, buff);
  write(1, buff, strlen(buff));

 
  
  

  /*mod = shmat(6, addr);
  ptr = (int*) mod;
  *ptr = 6;
  itoa (*ptr, buff);
  write(1, buff, strlen(buff));
  */



buff2 = "\nHacemos fork\n";
  write(1, buff2, strlen(buff2));
  int h= fork();
  
  
  if (h>0) exit();

  // buff2 = "\nMiramos que la pagina shared tenga el mismo valor\n";
  // write(1, buff2, strlen(buff2));
 
   mod1 = shmat(7, addr);
   int b=2;
  buff2 = "\nborramos asignacion mod1\n";
  write(1, buff2, strlen(buff2));
  int a = shmdt(mod1);
  // itoa(b, buff);//Este no funciona, el porque no lo se
  // write(1, buff, strlen(buff));
  itoa(a, buff);
  write(1, buff, strlen(buff));
  
  
  shmrm(7);
  

  buff2 = "\nBorramos la asignacion mod\n";
  write(1, buff2, strlen(buff2));
 
 
  a = shmdt(mod);
  itoa(a, buff);
  write(1, buff, strlen(buff));
  buff2 = "\nSi todo ok esto deberia de dar 0\n";
  write(1, buff2, strlen(buff2));
  mod = shmat(7, addr);
  ptr = (int*) mod;
  itoa (*ptr, buff);
  write(1, buff, strlen(buff));
  exit();


  
  


  while(1) { }
}

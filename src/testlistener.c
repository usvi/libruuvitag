#include <libruuvitag.h>

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>



int main()
{
  printf("Ruuvitag testlistener starting\n");
  u8LibRuuviTagInit("+CURRENT+NEW", "+CURRENT+NEW");

  // Emulate real program here
  while(1)
  {
    sleep(1);
  }
  
  u8LibRuuviTagDeinit();
  return 0;
}

#include <libruuvitag.h>

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>



int main()
{
  uint8_t u8_i;
  printf("Ruuvitag testlistener starting\n");
  u8LibRuuviTagInit("+CURRENT+NEW", "+CURRENT+NEW");

  for(u8_i = 30; u8_i > 0; u8_i--)
  {
    printf("Testlistener sleeping %" PRIu8 "\n", u8_i);
    sleep(1);
  }
  
  u8LibRuuviTagDeinit();
  return 0;
}

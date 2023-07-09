#include <libruuvitag.h>

#include <stdio.h>
#include <unistd.h>
#include <signal.h>


void vSigintHandler(int i_signal_id)
{
  ;
}


int main(void)
{
  libruuvitag_context_type* px_context = NULL;

  signal(SIGINT, vSigintHandler);
  
  printf("Ruuvitag testlistener starting\n");
  printf("Initializing library\n");
  px_context = pxLibRuuviTagInit("+CURRENT+NEW", "+CURRENT+NEW");

  if (px_context == NULL)
  {
    printf("Unable to initialize library\n");

    return 1;
  }
  
  // Emulate real program here
  pause();

  printf("\nDeinitializing library\n");
  vLibRuuviTagDeinit(px_context);
  
  return 0;
}

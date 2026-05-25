
#include "BAP.h"


/* Func */

void BAP_jump(void)
{

   __asm("       MOV  SP, #0x400");
   __asm("       LB   #" STR_VALUE(BL_IMG_APPLICATION_BEGIN_ADDR) );
}
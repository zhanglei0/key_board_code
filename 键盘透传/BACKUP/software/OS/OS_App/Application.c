
#include <rthw.h>
#include <rtthread.h>

#include "port.h"


int rt_application_init(void)
{
  SysTick_Configuration();
  return 0;
}
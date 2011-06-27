/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * Wed Apr 17 16:05:29 EDT 2002 (James Roth)
 *
 *  - Fixed an unlikely sys_thread_new() race condition.
 *
 *  - Made current_thread() work with threads which where
 *    not created with sys_thread_new().  This includes
 *    the main thread and threads made with pthread_create().
 *
 *  - Catch overflows where more than SYS_MBOX_SIZE messages
 *    are waiting to be read.  The sys_mbox_post() routine
 *    will block until there is more room instead of just
 *    leaking messages.
 */
#include "lwip/debug.h"

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include "lwip/sys.h"
#include "lwip/opt.h"
#include "lwip/stats.h"

#include "diosix.h"
#include "functions.h"
#include "roles.h"

/*-----------------------------------------------------------------------------------*/
int main()
{      
   /* name this process so others can find it */
   diosix_set_role(DIOSIX_ROLE_NETWORKSTACK);
   
   /* step into the correct layer - layer 3 */
   diosix_priv_layer_up(3);
   
   printf("lwip: TCPIP stack running\n");
   
   
}
/*-----------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------*/
u32_t sys_now(void)
{
   diosix_kernel_stats stats;
   
   diosix_get_kernel_stats(&stats);
   
   return stats.kernel_uptime;
}


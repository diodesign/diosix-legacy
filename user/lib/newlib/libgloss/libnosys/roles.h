/* user/lib/newlib/libc/sys/diosix-i386/roles.h
 * list of roles known to the kernel
 * Author : Chris Williams
 * Date   : Tues,7 Dec 2010.09:28:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _ROLES_H
#define   _ROLES_H

#define DIOSIX_ROLE_NONE             (0)

/* core system processes */
#define DIOSIX_ROLE_SYSTEM_EXECUTIVE (1) /* usually init */
#define DIOSIX_ROLE_VFS              (2) /* the virtual filesystem manager */
#define DIOSIX_ROLE_PAGER            (3) /* the secondary storage swapper */
#define DIOSIX_ROLE_NETWORKSTACK     (4) /* the TCP/IP networking stack */

/* basic 'console' hardware */
#define DIOSIX_ROLE_CONSOLEVIDEO     (5) /* default display hardware */
#define DIOSIX_ROLE_CONSOLEKEYBOARD  (6) /* default keyboard hardware */

/* system architecture */
#define DIOSIX_ROLE_PCIMANAGER       (7) /* the PCI bus driver */

/* total number of roles - not including DIOSIX_ROLE_NONE */
#define DIOSIX_ROLES_NR              (7)

#endif

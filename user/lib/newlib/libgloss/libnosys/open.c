/* user/lib/newlib/libgloss/libnosys/open.c
 * Open a file via the virtual file system.
 * Author : Chris Williams
 * Date   : Sat,25 Dec 2010.00:08:44

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

/* platform independent definitions */
#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>
#undef errno
extern int errno;

/* diosix-specific definitions */
#include "diosix.h"
#include "functions.h"

int
_DEFUN (_open, (file, flags, mode),
        char *file  _AND
        int   flags _AND
        int   mode)
{
   diosix_msg_info msg;
   diosix_msg_multipart req[VFS_OPEN_PARTS];
   diosix_vfs_request_head head;
   diosix_vfs_request_chown descr;
   diosix_vfs_reply reply;
   kresult err;

   /* sanity check */
   if(!file) return -1;
   
   /* craft a request to the vfs to change a file's ownership.
      include the null-terminating byte */
   descr.flags = flags;
   descr.mode = mode;
   descr.length = strlen(file) + sizeof(unsigned char);
   diosix_vfs_new_request(&req, open_req, &head, &descr,
                          sizeof(diosix_vfs_request_open));
   DIOSIX_WRITE_MULTIPART(&req, VFS_MSG_OPEN_PATH, file,
                          descr.length);

   /* create the rest of the message and send */
   err = diosix_vfs_request_msg(&msg, &req, VFS_OPEN_PARTS,
                                &reply, sizeof(diosix_vfs_reply));
   
   if(err || reply.result)
   {
      errno = ENOSYS;
      return -1;
   }
   
   return reply.posix_result; /* success - return file descriptor */
}

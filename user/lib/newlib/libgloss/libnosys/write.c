/* user/lib/newlib/libgloss/libnosys/write.c
 * Write to a file via the virtual file system.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#undef errno
extern int errno;

/* diosix-specific definitions */
#include "diosix.h"
#include "functions.h"
#include "io.h"

int
_DEFUN (_write, (file, ptr, len),
        int   file  _AND
        char *ptr   _AND
        int   len)
{
   /* structures to hold the message for the fs system */
   diosix_msg_info msg;
   diosix_msg_multipart req[VFS_WRITE_PARTS];
   diosix_vfs_request_head head;
   diosix_vfs_request_write descr;
   diosix_vfs_reply reply;
   kresult err;

   /* copy stdout to the debug channel */
   if(file == 1)
   {
      char *str;

      str = malloc(len + 1);
      if(str)
      {
         memcpy(str, ptr, len);
         str[len] = 0;
         diosix_debug_write(str);
         free(str);
      }
   }
   
   /* the pid of the filesystem that will carry out
      the write() for us */
   unsigned int fspid = diosix_vfs_get_fs(file);
   if(!fspid) return -1;
   
   /* craft a request to the vfs to write data to a file */
   descr.filedescr = file;
   descr.length = len;
   diosix_vfs_new_req(req, write_req, &head,
                      &descr, sizeof(diosix_vfs_request_write));
   
   /* add an entry into the multipart array to point to
      the data in memory to write to the file */
   DIOSIX_WRITE_MULTIPART(req, VFS_MSG_WRITE_DATA, ptr,
                          descr.length);
   
   /* create the rest of the message and send */
   err = diosix_vfs_send_req(fspid, &msg, req, VFS_WRITE_PARTS,
                             &reply, sizeof(diosix_vfs_reply));
   
   if(err || reply.result)
   {
      errno = ENOSYS;
      return -1;
   }
   
   return 0; /* success */
}

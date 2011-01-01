/* user/lib/newlib/libgloss/libnosys/link.c
 * Link a file via the virtual file system.
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
#include <string.h>
#include <errno.h>
#undef errno
extern int errno;

/* diosix-specific definitions */
#include "diosix.h"
#include "functions.h"
#include "io.h"

int
_DEFUN (_link, (existing, new),
        char *existing _AND
        char *new)
{
   /* structures to hold the message for the fs system */
   diosix_msg_info msg;
   diosix_msg_multipart req[VFS_LINK_PARTS];
   diosix_vfs_request_head head;
   diosix_vfs_request_link descr;
   diosix_vfs_reply reply;
   kresult err;

   /* craft a request to the vfs to link a file. don't forget
      the terminating byte in the strings */
   descr.existing_length = strlen(existing) + sizeof(unsigned char);
   descr.new_length = strlen(new) + sizeof(unsigned char);
   diosix_vfs_new_req(req, link_req, &head,
                      &descr, sizeof(diosix_vfs_request_link));
   
   /* add an entry into the multipart array to point to
      the path of the source file */
   DIOSIX_WRITE_MULTIPART(req, VFS_MSG_LINK_EXISTING, existing,
                          descr.existing_length);
   
   /* add an entry into the multipart array to point to
      the path of the new file */
   DIOSIX_WRITE_MULTIPART(req, VFS_MSG_LINK_NEW, new,
                          descr.new_length);
   
   /* create the rest of the message and send */
   err = diosix_vfs_send_req(0, &msg, req, VFS_LINK_PARTS,
                             &reply, sizeof(diosix_vfs_reply));
   
   if(err || reply.result)
   {
      errno = ENOSYS;
      return -1;
   }
   
   return 0; /* success */
}

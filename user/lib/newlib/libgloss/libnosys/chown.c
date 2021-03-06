/* user/lib/newlib/libgloss/libnosys/chown.c
 * Set the owner of a file via the virtual file system.
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
#include <sys/types.h>
#include <string.h>
#undef errno
extern int errno;

/* diosix-specific definitions */
#include "diosix.h"
#include "functions.h"
#include "io.h"

int
_DEFUN (_chown, (path, owner, group),
        const char *path  _AND
        uid_t owner _AND
        gid_t group)
{
   /* structures to hold the message for the fs system */
   diosix_msg_info msg;
   diosix_msg_multipart req[VFS_CHOWN_PARTS];
   diosix_vfs_request_head head;
   diosix_vfs_request_chown descr;
   diosix_vfs_reply reply;
   kresult err;

   /* sanity check */
   if(!path) return -1;
   
   /* craft a request to the vfs to change a file's ownership.
      include the null-terminating byte */
   descr.owner = owner;
   descr.group = group;
   descr.length = strlen(path) + sizeof(unsigned char);
   diosix_vfs_new_req(req, chown_req, &head, &descr,
                      sizeof(diosix_vfs_request_chown));
   
   /* add an entry into the multipart array to point to
      the chown() path */
   DIOSIX_WRITE_MULTIPART(req, VFS_MSG_CHOWN_PATH, path,
                          descr.length);

   /* create the rest of the message and send */
   err = diosix_vfs_send_req(0, &msg, req, VFS_CHOWN_PARTS,
                             &reply, sizeof(diosix_vfs_reply));
   
   if(err || reply.result)
   {
      errno = ENOSYS;
      return -1;
   }
   
   return 0; /* success */
}

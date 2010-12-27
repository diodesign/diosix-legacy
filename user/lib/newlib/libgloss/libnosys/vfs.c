/* user/lib/newlib/libgloss/libnosys/vfs.c
 * library functions to send and receive generic data via the vfs
 * Author : Chris Williams
 * Date   : Thurs,09 Dec 2009.04:45:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include "diosix.h"
#include "functions.h"
#include "roles.h"
#include "io.h"

/* ----------------------------------------------------
   suppport functions
   ------------------------------------------------- */

/* diosix_vfs_new_request
   Craft a new request to the vfs.
   => array = pointer to multipart array that the vfs
              message will be constructed out of.
              The first element is the request header,
              the second the request information. Further
              elements will be relevant to the specific
              request type.
      type = vfs request type
      head = pointer to structure holding the request's header
      request = pointer to the request description structure.
                if the pointer is null, then no description
                structure set up occurs.
      size = size of the request description structure
   <= returns 0 for success, or an error code
*/
kresult diosix_vfs_new_request(diosix_msg_multipart *array,
                               diosix_vfs_req_type type,
                               diosix_vfs_request_head *head,
                               void *descr, unsigned int size)
{
   /* sanity checks */
   if(!array || !head) return e_bad_params;

   head.type = type;
   DIOSIX_WRITE_MULTIPART(array, VFS_REQ_HEADER, head, sizeof(diosix_vfs_request_head));
   
   if(request)
   {
      /* check the size is sane */
      if(!size) return e_bad_params;
      
      DIOSIX_WRITE_MULTIPART(array, VFS_REQ_DESCRIBE, descr, size);
   }
   
   return success;
}

/* diosix_vfs_request_msg
   Fill out all the details to send a vfs request message
   and perform the send to the vfs process. This function
   will block until the vfs completes the action or wants
   to return an error.
   => msg = message structure to send
      parts = pointer to multiparts array of request's contents
      parts_count = number of parts in the array
      reply = pointer to structure in which the reply will be stored
      reply_size = size of the reply-holding structure
   <= 0 for success, or an error code
*/
kresult diosix_vfs_request_msg(diosix_msg_info *msg,
                               diosix_msg_multipart *parts,
                               unsigned int parts_count,
                               void *reply, unsigned int reply_size)
{
   /* sanity check */
   if(!msg || !parts || !parts_count || !reply || !reply_size)
      return e_bad_parmas;

   /* fill out all the details */
   msg->pid   = DIOSIX_MSG_ANY_PROCESS;
   msg->tid   = DIOSIX_MSG_ANY_THREAD;
   msg->role  = DIOSIX_ROLE_VFS;
   msg->flags = DIOSIX_MSG_GENERIC | DIOSIX_MSG_QUEUEME | DIOSIX_MSG_MULTIPART;
   msg->send  = parts;
   msg->send_size = parts_count;
   msg->recv  = reply;
   msg->recv_max_size = reply_size;
   
   /* send the message */
   return diosix_msg_send(msg);
}

/* ----------------------------------------------------
   client interface: manipulating file contents
   ------------------------------------------------- */

kresult diosix_io_open(char *filename, unsigned int flags, unsigned int mode)
{
   return success;
}

kresult diosix_io_close(void)
{
   return success;
}

kresult diosix_io_read(unsigned int flags, char *buffer, unsigned int size)
{
   return success;
}

kresult diosix_io_write(char *buffer, unsigned int size)
{
   return success;
}

kresult diosix_io_lseek(unsigned int file, unsigned int ptr, unsigned int dir)
{
   return success;
}

/* ----------------------------------------------------
   client interface: manipulating the filesystem
   ------------------------------------------------- */

kresult diosix_io_link(char *old, char *new)
{
   return success;
}

kresult diosix_io_unlink(char *victim)
{
   return success;
}

kresult diosix_io_fstat(int file, struct stat *st)
{
	return success;
}

kresult diosix_io_stat(const char *file, struct stat *st)
{
	return success;
}

/* ----------------------------------------------------
   server interface
   ------------------------------------------------- */
kresult diosix_io_register(diosix_server *server)
{
   return success;
}

kresult diosix_io_deregister(diosix_server *server)
{
   return success;
}

kresult diosix_io_get_work(diosix_server_request *job)
{
   return success;
}

kresult diosix_io_work_done(diosix_server_request *job)
{
   return success;
}

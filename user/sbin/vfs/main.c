/* user/sbin/vfs/main.c
 * Core functions for the virtual filesystem process
 * Author : Chris Williams
 * Date   : Sun,19 Dec 2010.04:27:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <stdlib.h>
#include <string.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"

/* -------------------------------------------------------------------------
   Filesystem tree management
   Using SGLib: http://sglib.sourceforge.net/doc/index.html
   ------------------------------------------------------------------------- */

typedef struct vfs_tree_node vfs_tree_node;
struct vfs_tree_node
{
   unsigned char *path; /* node pathname */
   unsigned int pid; /* process managing this node */
   
   /* child nodes */
   vfs_tree_node **children;
};


/* default size of the initial receive buffer */
#define RECEIVE_BUFFER_SIZE   (2048)

/* ------------------------------------------
   manage the filespace
   ------------------------------------------ */

/* fs_from_path
   Lookup the process managing the given path 
   => path = path to examine
   <= pid of FS process, or 0 for none
*/
unsigned int fs_from_path(char *path)
{
   return 0;
}

/* parent_fs_from_path
   Lookup the process managing the parent of the given path 
   => path = path to examine
   <= pid of FS process, or 0 for none
*/
unsigned int parent_fs_from_path(char *path)
{
   return 0;
}

/* register_process
   Add a process to the filespace (mount by any other name)
   => msg = message requesting to join the filespace
      path = root path of the filesystem/device
   <= 0 for success, or an error code
*/
kresult register_process(diosix_msg_info *msg, char *path)
{
   unsigned pid;
   
   /* sanity check */
   if(!path || !msg) return e_failure;
   
   /* identify the process managing the point in the
      filespace where this new filesystem or device
      would like to start */
   pid = fs_from_path(path);
   
   /* send a request to the parent fs process to check the
      requesting process has sufficient privileges */
   if(pid)
   {
      /* structures to send a message to the fs */
      diosix_msg_info req_msg;
      diosix_msg_multipart req[VFS_MOUNT_PARTS];
      diosix_vfs_request_head head;
      diosix_vfs_request_mount descr;
      diosix_vfs_reply reply;
      kresult err;
      
      /* tell the fs process about the requester.
         it may reject the request because the process
         has insufficient privileges. */
      descr.pid = msg->pid;
      descr.uid = msg->uid;
      descr.gid = msg->gid;
      descr.length = strlen(path) + sizeof(unsigned char);
      diosix_vfs_new_req(req, mount_req, &head, &descr,
                         sizeof(diosix_vfs_request_mount));
      
      /* pass the path to the fs process */
      DIOSIX_WRITE_MULTIPART(req, VFS_MSG_MOUNT_PATH, path,
                             descr.length);
      
      /* send the message and await a response */
      err = diosix_vfs_send_req(pid, &req_msg, req, VFS_MOUNT_PARTS,
                                &reply, sizeof(diosix_vfs_reply));
      
      /* did it work out OK? */
      if(err) return e_failure;
      if(reply.result) return reply.result;
   }
   else
   {
      /* no parent filesystem exists so provided the 
         requesting process is running as root, allow the
         request */
      if(msg->uid != DIOSIX_SUPERUSER_ID) return e_no_rights;
   }

   /* add the path to the filesystem */
   
   return success;
}

/* deregister_process
   Remove a process from the filespace (umount by any other name)
   => msg = message requesting to leave the filespace, or
            NULL to forcibly remove a filesystem or device
      path = root path of the filesystem/device
   <= 0 for success, or an error code
*/
kresult deregister_process(diosix_msg_info *msg, char *path)
{
   /* check the message sender is the process that maintains
      the given path */
   unsigned int ppid, pid = fs_from_path(path);
   
   /* if we can't find a process associated with the path
      then give up */
   if(!pid) return e_not_found;
   
   /* give up if the sender is not the owning filesystem
      unless we're forcibly removing it */
   if(msg && msg->pid != pid) return e_no_rights;
      
   /* find the parent fs so we can tell it about the unmount */
   ppid = parent_fs_from_path(path);
   
   /* if the parent pid and the owning pid are the same 
      then something's gone wrong */
   if(ppid == pid) return e_bad_params;
   
   if(ppid)
   {
      /* structures to send a message to the parent fs */
      diosix_msg_info req_msg;
      diosix_msg_multipart req[VFS_UMOUNT_PARTS];
      diosix_vfs_request_head head;
      diosix_vfs_request_umount descr;
      diosix_vfs_reply reply;
      kresult err;
      
      descr.length = strlen(path) + sizeof(unsigned char);
      
      diosix_vfs_new_req(req, umount_req, &head, &descr,
                         sizeof(diosix_vfs_request_mount));
      
      /* pass the path to the parent fs process */
      DIOSIX_WRITE_MULTIPART(req, VFS_MSG_UMOUNT_PATH, path,
                             descr.length);
      
      /* send the message and await a response */
      err = diosix_vfs_send_req(ppid, &req_msg, req, VFS_UMOUNT_PARTS,
                                &reply, sizeof(diosix_vfs_reply));
      
      /* did it work out OK? */
      if(err) return e_failure;
      if(reply.result) return reply.result;
   }
   
   /* remove the path */
   
   return success;
}

/* ------------------------------------------
   message interface with other processes
   ------------------------------------------ */

/* reply_to_request
   Send a reply to a vfs request
   => msg = message block to use, will contain
            the results from a diosix_msg_receive()
            that picked up the request.
      result = result of the request
*/
void reply_to_request(diosix_msg_info *msg, kresult result)
{
   diosix_vfs_reply reply;
   
   /* sanity check */
   if(!msg) return;
   
   /* ping a reply back to unlock the requesting thread */
   reply.result = result;
   msg->send = &reply;
   msg->send_size = sizeof(diosix_vfs_reply);
   
   diosix_msg_reply(msg);
}

/* wait_for_request
   Block until a request comes in */
void wait_for_request(void)
{
   diosix_msg_info msg;
   unsigned char recv_buffer[RECEIVE_BUFFER_SIZE];
   
   memset(&msg, 0, sizeof(diosix_msg_info));
   
   /* wait for a generic message to come in */
   msg.flags = DIOSIX_MSG_GENERIC;
   msg.recv = recv_buffer;
   msg.recv_max_size = RECEIVE_BUFFER_SIZE;
   
   if(diosix_msg_receive(&msg) == success)
   {
      diosix_vfs_request_head *req_head = (diosix_vfs_request_head *)recv_buffer;
      
      /* we cannot trust anything in the message payload */
      if(VFS_MSG_SIZE_CHECK(msg, 0, 0))
      {
         /* malformed request, it's too small */
         reply_to_request(&msg, e_too_small);
         return;
      }
      
      /* decode the request type */
      switch(req_head->type)
      {
         case chown_req:
         case close_req:
         case fstat_req:
         case link_req:
         case lseek_req:
         case open_req:
         case read_req:
         case readlink_req:
         case stat_req:
         case symlink_req:
         case unlink_req:
         case write_req:

         /* request to register a filesystem/device in the vfs filespace */
         case register_req:
         {
            kresult result;
            
            /* extract the request's data from the message payload */
            diosix_vfs_request_register *req_info = VFS_MSG_EXTRACT(req_head, 0);
            char *path = VFS_MSG_EXTRACT(req_head, sizeof(diosix_vfs_request_register));
            
            /* the payload might not be big enough to contain the request details */
            if(VFS_MSG_SIZE_CHECK(msg, 0, sizeof(diosix_vfs_request_register)))
            {
               reply_to_request(&msg, e_too_big);
               return;
            }
            
            /* we cannot trust the string length parameter in the payload, so check it */
            if(VFS_MSG_SIZE_CHECK(msg, sizeof(diosix_vfs_request_register), req_info->length))
            {
               reply_to_request(&msg, e_too_big);
               return;
            }
            
            /* is the path zero-terminated? */
            if(path[req_info->length])
            {
               reply_to_request(&msg, e_bad_params);
               return;
            }
            
            /* all seems clear */
            result = register_process(&msg, path);
            reply_to_request(&msg, result);

         }
         break;

         /* deregister a filesystem/device in the vfs filespace */
         case deregister_req:
         {
            kresult result;
            diosix_vfs_request_register *req_info = VFS_MSG_EXTRACT(req_head, 0);
            char *path = (char *)VFS_MSG_EXTRACT(req_head, sizeof(diosix_vfs_request_register));
            
            /* the payload might not be big enough to contain the request details */
            if(VFS_MSG_SIZE_CHECK(msg, 0, sizeof(diosix_vfs_request_register)))
            {
               reply_to_request(&msg, e_too_big);
               return;
            }
            
            /* we cannot trust the string length parameter in the payload, so check it */
            if(VFS_MSG_SIZE_CHECK(msg, sizeof(diosix_vfs_request_register), req_info->length))
            {
               reply_to_request(&msg, e_too_big);
               return;
            }
            
            result = deregister_process(&msg, path);
            reply_to_request(&msg, result);
         }
         break;
            
         /* if the type is unknown then fail it */
         default:
            reply_to_request(&msg, e_bad_params);
      }
   }
}

/* ------------------------------------------
   greetings, program
   ------------------------------------------ */

int main(void)
{
   /* name this process so others can find it */
   diosix_set_role(DIOSIX_ROLE_VFS);

   /* step into the correct layer - layer 3 */
   diosix_priv_layer_up(3);
   
   /* fork so that we have multiple worker threads */
   diosix_thread_fork(); diosix_thread_fork();
   
   /* get some work to do */
   while(1) wait_for_request();
}

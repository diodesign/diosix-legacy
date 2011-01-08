/* user/sbin/vfs/vfstree.c
 * Core functions for the virtual filesystem process
 * Author : Chris Williams
 * Date   : Sat,08 Jan 2011.16:21:00

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

#include "vfs.h"

/* pointer to the tree root node */
vfs_tree_node *vfs_tree_root;

/* provide simple atomic locking of the tree structure */
volatile unsigned int vfs_tree_lock = 0;

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
   kresult result = success;
   
   /* make this thread-safe */
   DIOSIX_SPINLOCK_ACQUIRE(&vfs_tree_lock);
   
   DIOSIX_SPINLOCK_RELEASE(&vfs_tree_lock);
   return result;
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
   unsigned int pid;
   vfs_tree_node *tree_node;
   kresult result;
   
   /* set this to 1 when we're process the last
    component in the path */
   unsigned char at_final_leafnode = 0;
   
   /* sanity check - no null pointers nor empty paths */
   if(!path || !msg) return e_failure;
   if(!(*path)) return e_bad_params;
   
   /* if the first character isn't / (and thus this isn't a
      full pathname) then give up */
   if(*path != '/') return e_bad_params;
   
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

   /* make this thread-safe */
   DIOSIX_SPINLOCK_ACQUIRE(&vfs_tree_lock);

   /* get the start of the vfs tree structure */
   tree_node = vfs_tree_root;
   
   /* if the root doesn't exist then create it */
   if(!tree_node)
   {
      tree_node = malloc(sizeof(vfs_tree_node));
      if(!tree_node)
      {
         result = e_failure;
         goto register_process_exit;
      }
      
      tree_node->path = strdup("/");
      if(!(tree_node->path))
      {
         result = e_failure;
         goto register_process_exit;
      }
      
      /* indicate that no process owns this (yet) and
         it has no child leafnodes */
      tree_node->pid = 0;
      tree_node->child_count = 0;
      tree_node->children = NULL;
   }
   
   /* skip over the initial / character */
   path++;
   
   /* add the path to the filesystem by breaking the path
      down into component leafnames */
   while(!at_final_leafnode)
   {
      /* record the start of the leafname */
      const char *leafname = path;
      vfs_tree_node *next_node = NULL;
      
      /* search forward to a null or a '/' terminator */
      while(*path != '\0' && *path != '/') path++;

      /* ascertain whether or not we're at the last leafname */
      if(*path == '\0')
      {
         at_final_leafnode = 1;
      }
      else
      {
         /* if the byte after the / is a null then effectively
            strip the trailing / */
         if(path[1] == '\0') at_final_leafnode = 1;
       
         /* terminate the leafname so that it can be used as a 
            standalone string */
         *path = '\0';
      }
      
      /* try to find a match in the current node's children */
      if(tree_node->child_count)
      {
         unsigned int child_loop;

         for(child_loop = 0; child_loop < tree_node->child_count; child_loop++)
         {
            if(strcmp(leafname, tree_node->children[child_loop].path) == 0)
            {
               /* got a match */
               next_node = &(tree_node->children[child_loop]);
               break;
            }
         }
      }

      /* can't allow any collisions of the final leafname in the tree */
      if(next_node && at_final_leafnode)
      {
         result = e_exists;
         goto register_process_exit;
      }

      /* if there's no match, create a new child slot for this new node */
      if(!next_node)
      {
         vfs_tree_node *new_children;
         
         new_children = realloc(tree_node->children,
                                sizeof(vfs_tree_node) * (tree_node->child_count + 1));
         
         if(!new_children)
         {
            result = e_failure;
            goto register_process_exit;
         }
         
         /* update the pointers... */
         tree_node->children = new_children;
         next_node = &(tree_node->children[tree_node->child_count]);

         /* ...and the total number of children */
         tree_node->child_count++;
         
         if(!next_node)
         {
            result = e_failure;
            goto register_process_exit;
         }
         
         next_node->children = NULL;
         next_node->child_count = 0;
         
         /* copy the leafname into the new node */
         next_node->path = strdup(leafname);
         if(!(next_node->path))
         {
            free(next_node);
            result = e_failure;
            goto register_process_exit;
         }
         
         /* have we finished building the tree? */
         if(at_final_leafnode)
         {
            next_node->pid = msg->pid;
            result = success;
            goto register_process_exit;
         }
         else
            /* if not then no one can claim this node */
            next_node->pid = 0;
      }
      
      /* prepare for next iteration */
      tree_node = next_node;
      path++;
   }
   
register_process_exit:
   DIOSIX_SPINLOCK_RELEASE(&vfs_tree_lock);
   return result;
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
   unsigned int ppid, pid;
   
   /* sanity check - no null pointers nor empty paths */
   if(!path || !msg) return e_failure;
   if(!(*path)) return e_bad_params;
   
   /* if the first character isn't / then give up */
   if(*path != '/') return e_bad_params;
   
   /* check the message sender is the process that maintains
      the given path */
   pid = fs_from_path(path);
   
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

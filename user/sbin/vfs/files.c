/* user/sbin/vfs/files.c
 * Manage files and file handles in the vfs
 * Author : Chris Williams
 * Date   : Sat,15 Jan 2011.16:06:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"

#include "vfs.h"

vfs_process *process_table[PROCESS_HASH_SIZE];

/* provide simple atomic locking of the process table structure */
volatile unsigned int process_table_lock = 0;

/* assumes: 0 = stdin, 1 = stdout, 2 = stderr, 3+ = available */
#define FIRST_FILEDESC  (STDERR_FILENO + 1)

/* ------------------------------------------
   manage file handles
   ------------------------------------------ */

/* new_filedesc
   Assign a new file handle number for a given process
   => pid = process to generate the handle number for
      fspid = filesystem to associate with the handle
   <= file handle or 0 for a failure
*/
int new_filedesc(unsigned int pid, unsigned int fspid)
{
   vfs_process *search;
   unsigned int loop;
   
   /* sanity check */
   if(!pid) return 0;
   
   /* make this thread-safe */
   DIOSIX_SPINLOCK_ACQUIRE(&process_table_lock);
   
   /* find the pid in the hash table */
   search = process_table[pid % PROCESS_HASH_SIZE];
   while(search)
   {
      if(search->pid == pid) break;
      search = search->next;
   }
   
   /* if search is unset then create an entry for it */
   if(!search)
   {
      search = malloc(sizeof(vfs_process));
      if(!search)
      {
         DIOSIX_SPINLOCK_RELEASE(&process_table_lock);
         return 0;
      }
      
      search->pid = pid;
      search->highest_filedesc = 0;
      search->available_filedesc = 0;
      search->filedesc_table = NULL;
      search->prev = NULL;
      
      /* add it to the hash table */
      if(process_table[pid % PROCESS_HASH_SIZE])
      {
         search->next = process_table[pid % PROCESS_HASH_SIZE];
         search->next->prev = search;
      }
      
      process_table[pid % PROCESS_HASH_SIZE] = search;
   }
   
   /* if we're out of available descriptor slots then increase the table */
   if(!(search->available_filedesc))
   {
      unsigned int *newptr;
      unsigned int newsize = search->highest_filedesc * 2;
      
      if(!newsize) newsize = 2;
      
      newptr = realloc(search->filedesc_table, newsize * sizeof(unsigned int));
      if(!newptr)
      {
         DIOSIX_SPINLOCK_RELEASE(&process_table_lock);
         return 0;
      }
      search->filedesc_table = newptr;
      
      if(search->highest_filedesc)
         search->available_filedesc = search->highest_filedesc;
      else
         search->available_filedesc = newsize;
         
      /* zero the new part of the table and set highest_filedesc to
         the new max size of the table */
      while(search->highest_filedesc < newsize)
         search->filedesc_table[search->highest_filedesc++] = 0;
   }
   
   /* get the lowest available file handle */
   for(loop = 0; loop < search->highest_filedesc; loop++)
   {
      if(!(search->filedesc_table[loop]))
      {
         search->filedesc_table[loop] = fspid;
         search->available_filedesc--;
         
         /* return the table index as a file handle, skipping
            over the stdin/stdout/stderr entries */
         DIOSIX_SPINLOCK_RELEASE(&process_table_lock);
         return loop + FIRST_FILEDESC;
      }
   }
   
   /* why are we still here? something must have gone wrong */
   DIOSIX_SPINLOCK_RELEASE(&process_table_lock);
   return 0;
}

/* destroy_filedesc
    Delete a file handle number from a given process
    => pid = process to remove the handle number from
       filedesc = handle to delete
    <= 0 for success, or an error code
*/
kresult destroy_filedesc(unsigned int pid, int filedesc)
{
   vfs_process *search;
   
   /* make this thread-safe */
   DIOSIX_SPINLOCK_ACQUIRE(&process_table_lock);

   search = process_table[pid % PROCESS_HASH_SIZE];
   while(search)
   {
      if(search->pid == pid) break;
      search = search->next;
   }
   
   /* bail out if the process isn't in the hash table */
   if(!search)
   {
      DIOSIX_SPINLOCK_RELEASE(&process_table_lock);
      return e_not_found;
   }

   /* sanity check the file handle */
   if(filedesc < FIRST_FILEDESC || filedesc >= search->highest_filedesc)
   {
      DIOSIX_SPINLOCK_RELEASE(&process_table_lock);
      return e_bad_params;
   }
   
   /* clear the file handle and mark it as available */
   search->filedesc_table[filedesc - FIRST_FILEDESC] = 0;
   search->available_filedesc++;
   
   DIOSIX_SPINLOCK_RELEASE(&process_table_lock);
   return success;
}


/* ------------------------------------------
   handle files being opened and closed
   ------------------------------------------ */

/* open_file
   Process a file open request by generating a new file handle and sending it onto
   the responsible filesystem or driver process
   => msg = message block asking for the file to be opened
      flags, mode = flags and mode from the original open() call
      path = pointer to path string for the file
      reply = block into which the results of the request will be stored, if the
              request was successful
   <= success, or an error code
*/
kresult open_file(diosix_msg_info *msg, int flags, int mode, char *path, diosix_vfs_pid_reply *reply)
{
   /* structures for message to send on to the fs/driver */
   diosix_msg_info req_msg;
   diosix_msg_multipart req[VFS_OPEN_PARTS];
   diosix_vfs_request_head head;
   diosix_vfs_request_open descr;
   
   unsigned int fspid;
   int filedesc;
   kresult err;
      
   /* sanity check */
   if(!msg || !path || !reply) return e_bad_params;

   /* get a filesystem pid for the path */
   fspid = fs_from_path(path);
   if(!fspid) return e_not_found;
      
   /* generate a new file handle for this process */
   filedesc = new_filedesc(msg->pid, fspid);
   if(!filedesc) return e_failure;
      
   /* send message to fs/driver process */
   descr.flags = flags;
   descr.mode = mode;
   descr.length = strlen(path) + sizeof(unsigned char);
   descr.pid = msg->pid;
   descr.uid = msg->uid;
   descr.gid = msg->gid;
   descr.filedesc = filedesc;
   
   diosix_vfs_new_req(req, open_req, &head, &descr,
                      sizeof(diosix_vfs_request_open));
   
   /* pass the path to the fs process */
   DIOSIX_WRITE_MULTIPART(req, VFS_MSG_OPEN_PATH, path,
                          descr.length);
   
   /* send the message and await a response */
   err = diosix_vfs_send_req(fspid, &req_msg, req, VFS_MOUNT_PARTS,
                             &reply, sizeof(diosix_vfs_reply));
   
   /* either return an error code from the fs/driver or send back
      the pid and file handle */
   if(err)
   {
      destroy_filedesc(msg->pid, filedesc);
      return err;
   }
   else
   {
      reply->fspid = fspid;
      reply->filedesc = filedesc;
      return success;
   }
}

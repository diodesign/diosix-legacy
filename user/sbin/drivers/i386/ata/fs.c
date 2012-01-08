/* user/sbin/drivers/i386/ata/fs.c
 * Driver for ATA-connected storage devices 
 * Author : Chris Williams
 * Date   : Sun,16 Jan 2011.22:54:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"

#include "ata.h"
#include "lowlevel.h"

#define RECEIVE_BUFFER_SIZE      (2048)

/* define the number of hash buckets and the formula.
   DEVICE_HASH_CALC(pid, filehandle) attempts to calculate a
   hash table entry from the pid and filehandle */
#define DEVICE_HASH_MAX          256
#define DEVICE_HASH_CALC(p, f)   ((((p) * 5) + (f)) % DEVICE_HASH_MAX)

/* hash table of PID+filehandle to device pointer */
typedef struct device_hash_entry device_hash_entry;
struct device_hash_entry
{
   ata_device *device;
   
   /* link to other entries in this hash bucket */
   device_hash_entry *prev, *next;
};

device_hash_entry *pid_to_device_hash[DEVICE_HASH_MAX];

/* add_to_device_hash
   Add a device to a pid-to-device hash table. Assumes it is
   safe to manipulate the data structures - ie: a lock is being held
   => pid, filedesc = pid and filehandle to associate
      device = pointer to device structure
   <= 0 for success, or an error code
*/
kresult add_to_device_hash(unsigned int pid, int filedesc, ata_device *device)
{
   device_hash_entry *head = pid_to_device_hash[DEVICE_HASH_CALC(pid, filedesc)];
   
   device_hash_entry *new = malloc(sizeof(device_hash_entry));
   if(!new) return e_failure;
   
   new->device = device;
   new->prev = NULL;
   
   /* insert it at the head */
   if(head) head->prev = new;
   new->next = head;
   pid_to_device_hash[DEVICE_HASH_CALC(pid, filedesc)] = new;
   
   printf("add_to_device_hash: success: pid %i filedesc %i device %p hash %i\n",
          pid, filedesc, device, DEVICE_HASH_CALC(pid, filedesc));
   
   return success;
}

/* get_device_from_pid
   Lookup a device from the PID and file handle that's should
   be registered to it
   => pid, filedesc = pid and filehandle associated with the device to find
   <= pointer to device structure, or NULL for not found
*/
ata_device *get_device_from_pid(unsigned int pid, int filedesc)
{
   device_hash_entry *head = pid_to_device_hash[DEVICE_HASH_CALC(pid, filedesc)];
   
   /* search through the hash table entries looking for our device */
   while(head)
   {
      ata_device *device = head->device;
      
      if(device->pid == pid && device->filedesc == filedesc) return device;
      head = head->next;
   }
   
   /* fall through to returning a not-found NULL */
   return NULL;
}

/* link_device_to_pid
   Associate an ATA device with a given PID and filehandle, so this
   can be used to auth requests in future
   => pid = ID of process allowed to access the device
      filedesc = filehandle assigned by the VFS for this PID and device
      controller, channel, device = select the device to link
   <= 0 for success, or an error code
*/
kresult link_device_to_pid(unsigned int pid, int filedesc, unsigned char controller, unsigned char channel, unsigned char device)
{
   /* sanity checks */
   if(!pid || filedesc < 0) return e_bad_params;
   if(controller >= ATA_MAX_CONTROLLERS || device >= ATA_MAX_DEVS_PER_CHANNEL || channel >= ATA_MAX_CHANS_PER_CNTRLR)
      return e_bad_params;
      
   /* acquire lock */
   lock_spin(&ata_lock);
   
   /* check that the device isn't claimed */
   if(controllers[controller].channels[channel].drive[device].pid == 0)
   {
      kresult err;
      
      /* record the PID and filehandle in the device's structure */
      controllers[controller].channels[channel].drive[device].pid = pid;
      controllers[controller].channels[channel].drive[device].filedesc = filedesc;
      
      /* add it to the hash table */
      err = add_to_device_hash(pid, filedesc,
                               &(controllers[controller].channels[channel].drive[device]));

      /* unlock critical lock and pass back success/error code */
      unlock_spin(&ata_lock);
      return err;
   }

   /* fall through to releasing lock and returning error message */
   unlock_spin(&ata_lock);
   return e_no_rights;
}

/* fs_init
   Initialise the filesystem-facing side of the process */
void fs_init(void)
{
   /* clear the hash table */
   memset(pid_to_device_hash, 0, sizeof(device_hash_entry *) * DEVICE_HASH_MAX);
}

/* ----------------------------------------------------------------------
                        main loop of the driver 
   ---------------------------------------------------------------------- */
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
            
      if(msg.recv_size < sizeof(diosix_vfs_request_head))
      {
         /* malformed request, it's too small to even hold a request type */
         reply_to_request(&msg, e_too_small);
         return;
      }
      
      /* sanity check the message header */
      if(req_head->magic != VFS_MSG_MAGIC)
      {
         /* malformed request, bad magic */
         reply_to_request(&msg, e_bad_magic);
         return;
      }
                  
      /* decode the request type */
      switch(req_head->type)
      {
         /* discard requests that aren't appropriate */
         case link_req:
         case readlink_req:
         case unlink_req:
         case symlink_req:
         case lseek_req:
         case register_req:
         case deregister_req:
            
         /* discard requests that aren't implemented (yet) */
         case chown_req:
         case fstat_req:
         case stat_req:
            reply_to_request(&msg, e_no_handler);
            break;

         /* open device if the request came from the VFS running as root */
         case open_req:
            if(msg.role == DIOSIX_ROLE_VFS && msg.uid == DIOSIX_SUPERUSER_ID)
            {
               unsigned char controller, channel, device;
               
               /* gather data from the vfs */
               diosix_vfs_request_head *req_head = (diosix_vfs_request_head *)recv_buffer;
               diosix_vfs_request_open *req = VFS_MSG_EXTRACT(req_head, 0);
               char *path = VFS_MSG_EXTRACT(req_head, sizeof(diosix_vfs_request_open));
               
               /* record the file handle against the PID and drive number by searching
                  through the ata path and move the pointer to the end of the base pathname */
               path = (char *)((unsigned int)strstr(path, ATA_PATHNAME_BASE) + strlen(ATA_PATHNAME_BASE));
               
               /* The format should be /controller/channel/device where each selector is a digit 0-9
                  skip the leading / and get the channel number */
               if(strlen("/0/0/0") == strlen(path) && *path == '/')
               {
                  path++;
                  
                  /* grab the controller number */
                  if(*path >= '0' && *path <= '9')
                  {
                     controller = *path++ - '0';
                  
                     if(*path == '/')
                     {
                        /* skip the next / and get the channel number */
                        path++;
                        
                        if(*path >= '0' && *path <= '9')
                        {
                           channel = *path++ - '0';
                           
                           /* skip the next / and get the device number */
                           path++;
                           
                           if(*path >= '0' && *path <= '9')
                           {
                              device = *path - '0';
                              reply_to_request(&msg, link_device_to_pid(req->pid,
                                                                        req->filedesc,
                                                                        controller, channel, device));
                              return;
                           }
                        }
                     }
                  }
               }
               
               /* fall through to failure */
               reply_to_request(&msg, e_bad_params);
            }
            else
               reply_to_request(&msg, e_no_rights);
            break;

         case read_req:
         {
            diosix_vfs_request_read *req = VFS_MSG_EXTRACT(req_head, 0);
            
            printf("read request from pid %i filehandle %i device %p!\n", msg.pid, req->filedes,
                   get_device_from_pid(msg.pid, req->filedes));
            
            reply_to_request(&msg, e_failure);
            break;
         }

         case close_req:
         case write_req:
            
         /* if the type is unknown then fail it */
         default:
            reply_to_request(&msg, e_bad_params);
      }
   }
}

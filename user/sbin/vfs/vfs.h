/* user/sbin/vfs/vfs.h
 * Core definitions for the virtual filesystem process
 * Author : Chris Williams
 * Date   : Sat,08 Jan 2011.16:08:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _VFS_H
#define  _VFS_H

/* describe the vfs tree */
typedef struct vfs_tree_node vfs_tree_node;
struct vfs_tree_node
{
   char *path; /* node pathname */
   unsigned int pid; /* process managing this node */
   
   /* child nodes */
   unsigned int child_count;
   vfs_tree_node *children;
};

#define PROCESS_HASH_SIZE     (128)

/* describe a process with open files */
typedef struct vfs_process vfs_process;
struct vfs_process
{
   unsigned int pid;
   
   /* describe a table of pids that match a given file handle */
   unsigned int available_filedesc; /* total available */
   unsigned int highest_filedesc; /* table count */
   unsigned int *filedesc_table;
   
   /* linked list within hash table */
   vfs_process *prev, *next;
};

/* default size of the initial receive buffer */
#define RECEIVE_BUFFER_SIZE   (2048)

/* in main.c */
void reply_to_request(diosix_msg_info *msg, kresult result);
void reply_to_pid_request(diosix_msg_info *msg, diosix_vfs_pid_reply *reply);
void wait_for_request(void);

/* in vfstree.c */
unsigned int fs_from_path(char *path);
unsigned int parent_fs_from_path(char *path);
kresult register_process(diosix_msg_info *msg, char *path);
kresult deregister_process(diosix_msg_info *msg, char *path);

/* in files.c */
kresult open_file(diosix_msg_info *msg, int flags, int mode, char *path, diosix_vfs_pid_reply *reply);

#endif
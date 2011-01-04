/* user/lib/newlib/libgloss/libnosys/io.h
 * structures and defines for input and output via the vfs
 * Author : Chris Williams
 * Date   : Thu,09 Dec 2010.05:09:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _IO_H
#define   _IO_H

/* agree on a way to represent non-ASCII key codes.
   8-bit ascii in the low byte with flags in the upper 24 bits */
#define KEY_TAB      (9)   /* tab control code */
#define KEY_ENTER    (13)  /* carriage return control code */
#define KEY_ESC      (27)
#define KEY_DELETE   (127)
#define KEY_F1       (1 | KEY_FKEY)
#define KEY_F2       (2 | KEY_FKEY)
#define KEY_F3       (3 | KEY_FKEY)
#define KEY_F4       (4 | KEY_FKEY)
#define KEY_F5       (5 | KEY_FKEY)
#define KEY_F6       (6 | KEY_FKEY)
#define KEY_F7       (7 | KEY_FKEY)
#define KEY_F8       (8 | KEY_FKEY)
#define KEY_F9       (9 | KEY_FKEY)
#define KEY_F10      (10 | KEY_FKEY)

#define KEY_SHIFT    (1 << 8)
#define KEY_BSPACE   (1 << 9)
#define KEY_CONTROL  (1 << 10)
#define KEY_LCTRL    (KEY_CONTROL)
#define KEY_RCTRL    (KEY_CONTROL)
#define KEY_LSHIFT   (KEY_SHIFT)
#define KEY_RSHIFT   (KEY_SHIFT)
#define KEY_ALT      (1 << 11)
#define KEY_LALT     (KEY_ALT)
#define KEY_RALT     (KEY_ALT)
#define KEY_CLOCK    (1 << 12)
#define KEY_FKEY     (1 << 13)
#define KEY_HOME     (1 << 14)
#define KEY_UP       (1 << 15)
#define KEY_DOWN     (1 << 16)
#define KEY_LEFT     (1 << 17)
#define KEY_RIGHT    (1 << 18)
#define KEY_PGUP     (1 << 19)
#define KEY_PGDOWN   (1 << 20)
#define KEY_INSERT   (1 << 21)
#define KEY_END      (1 << 22)

/* reserved at this time */
#define KEY_PSCREEN  (0)
#define KEY_SLOCK    (0)
#define KEY_NLOCK    (0)
#define KEY_BREAK    (0)

/* describe a filehandle-to-fs association */
typedef struct diosix_vfs_handle_assoc diosix_vfs_handle_assoc;
struct diosix_vfs_handle_assoc
{
   unsigned int filedesc;  /* the file handle */
   unsigned int pid;       /* the fs handling it */
   
   /* the associations are usually stored in a double
      linked list from a hash table */
   diosix_vfs_handle_assoc *prev, *next;
};

/* define vfs multipart message layout */
#define VFS_REQ_HEADER        (0) /* start with a header */
#define VFS_REQ_REQUEST       (1) /* next add information describing the request */
#define VFS_MSG_MAGIC         (0xd1000001)

/* request-specific elements in a multipart message */
#define VFS_MSG_CHOWN_PATH    (2)
#define VFS_MSG_LINK_EXISTING (2)
#define VFS_MSG_LINK_NEW      (3)
#define VFS_MSG_OPEN_PATH     (2)
#define VFS_MSG_READLINK_PATH (2)
#define VFS_MSG_STAT_PATH     (2)
#define VFS_MSG_UNLINK_PATH   (2)
#define VFS_MSG_WRITE_DATA    (2)
#define VFS_MSG_REGISTER_PATH (2)
#define VFS_MSG_MOUNT_PATH    (2)
#define VFS_MSG_UMOUNT_PATH   (2)

/* total number of parts for these requests */
#define VFS_CHOWN_PARTS       (3)
#define VFS_CLOSE_PARTS       (2)
#define VFS_FSTAT_PARTS       (2)
#define VFS_LINK_PARTS        (4)
#define VFS_LSEEK_PARTS       (2)
#define VFS_OPEN_PARTS        (3)
#define VFS_READ_PARTS        (3)
#define VFS_READLINK_PARTS    (3)
#define VFS_STAT_PARTS        (3)
#define VFS_UNLINK_PARTS      (3)
#define VFS_WRITE_PARTS       (3)
#define VFS_REGISTER_PARTS    (3)
#define VFS_MOUNT_PARTS       (3)
#define VFS_UMOUNT_PARTS      (3)

/* do some pointer math to get the contents of a request message
   base = pointer to start of a request message data
   offset = offset after the request header in bytes
   <= pointer (as a uint) within the request message data */
#define VFS_MSG_EXTRACT(base, offset) ( (unsigned int)(base) + sizeof(diosix_vfs_request_head) + (unsigned int)(offset) )

/* define vfs request message types */
typedef enum
{
   /* posix-style file IO requests */
   chown_req,
   close_req,
   fstat_req,
   link_req,
   lseek_req,
   open_req,
   read_req,
   readlink_req,
   stat_req,
   symlink_req,
   unlink_req,
   write_req,
   
   /* unix-world requests */
   mount_req,
   umount_req,
   
   /* diosix-specific requests */
   register_req,
   deregister_req
} diosix_vfs_req_type;

/* define the interface between clients and servers.
   each request is made up of a header and a payload
   of data. the total size of the request will be in  
   the headers for the ipc message. */
typedef struct
{
   unsigned int magic; /* to verify the version and sanity */
   diosix_vfs_req_type type;
} diosix_vfs_request_head;

/* define the structure of the vfs requests */
/* chown requires new owner and group IDs and a path length */
typedef struct
{
   unsigned int owner, group;
   unsigned int length;
} diosix_vfs_request_chown;

/* close, read and fstat require a file handle */
typedef struct
{
   int filedes; /* open file descriptor */
} diosix_vfs_request_close,
  diosix_vfs_request_read,
  diosix_vfs_request_fstat;

/* link requires two path lengths */
typedef struct
{
   unsigned int existing_length, new_length;
} diosix_vfs_request_link;

/* lseek requires a file handle, a new pointer and a flag */
typedef struct
{
   unsigned int filedesc, ptr, dir;
} diosix_vfs_request_lseek;

/* open requires a flag word, a mode word and a path length */
typedef struct
{
   int flags, mode, length;
   
   /* these are used only for vfs->fs communication to
      handle the open request and are ignored by clients */
   unsigned int pid; /* pid of requesting process */
   unsigned int uid; /* uid of requesting process */
   int filesdesc; /* file handle allocated by the vfs */
} diosix_vfs_request_open;

/* readlink, stat, unlink and umount require a path length */
typedef struct
{
   unsigned int length;
} diosix_vfs_request_readlink,
  diosix_vfs_request_stat,
  diosix_vfs_request_unlink,
  diosix_vfs_request_register,
  diosix_vfs_request_umount;

/* write requires a file handle and a source buffer length */
typedef struct
{
   unsigned int filedescr, length;
} diosix_vfs_request_write;

/* a mount request requires the new fs/device driver's pid,
   effective uid and gid and the path length size */
typedef struct
{
   unsigned int pid, uid, gid, length;
} diosix_vfs_request_mount;

/* a reply from the vfs has a header and a possible payload of data. */
typedef struct
{
   kresult result;  /* the diosix error or success code */
} diosix_vfs_reply;

/* a reply from the vfs has a header and a possible payload of data. */
typedef struct
{
   kresult result;  /* the diosix error or success code */
   
   /* the PID of a filesystem process relevant to the vfs call.
    when a file is asked to be opened, the vfs will generate 
    a per-process file handle in conlusion with the relevant fs.
    this fs's PID is passed back to the caller for future use. */
   unsigned int fspid;
   int filedesc; /* the file handle */
} diosix_vfs_pid_reply;

#endif

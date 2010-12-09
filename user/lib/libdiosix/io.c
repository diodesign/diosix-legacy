/* user/lib/libdiosix/console.c
 * library functions to send and receive generic data via the vfs
 * Author : Chris Williams
 * Date   : Thurs,09 Dec 2009.04:45:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <diosix.h>
#include <functions.h>
#include <io.h>

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

/* kresult diosix_io_fstat(int file, struct stat *st)
{
	return success;
}

kresult diosix_io_stat(const char *file, struct stat *st)
{
	return success;
} */

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

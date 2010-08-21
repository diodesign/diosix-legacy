/* kernel/ports/i386/include/boot.h
 * prototypes and structures for the i386 port of the kernel 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _BOOT_H
#define	_BOOT_H

/* welcome fluff */
#define PORT_BANNER	"\n" \
							" _|_|_|    _|    _|_|      _|_|_|  _|            \n" \
							" _|    _|      _|    _|  _|            _|    _|  \n" \
							" _|    _|  _|  _|    _|    _|_|    _|    _|_|    \n" \
							" _|    _|  _|  _|    _|        _|  _|  _|    _|  \n" \
							" _|_|_|    _|    _|_|    _|_|_|    _|  _|    _|  \n\n" \
							" Copyright (c) Chris Williams and contributors, 2009-2010.\n" \
							" See http://diodesign.co.uk/ for usage and licence.\n"

/* kernel payload */
extern unsigned int payload_modulemax;
mb_module_t *payload_readmodule(unsigned int modulenum);
payload_type payload_parsemodule(mb_module_t *module, payload_descr *payload);
kresult payload_preinit(multiboot_info_t *mbd);
kresult payload_exist_here(unsigned int ptr);

#endif

/* user/bin/testsuite/defs.h
 * Core definitions for the userland testsuite
 * Author : Chris Williams
 * Date   : Mon,9 May 2011.12:25:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _DEFS_H
#define  _DEFS_H

/* prepended to each output line to be picked up by the testsuite log processor */
#define LOG "__DTS__"
#define LOG_MAX_LINE_LENGTH (200)

/* define a test's structure */
typedef struct
{
   int (*func)(void); /* test function */
   const char *comment; /* human-readable string for the log */
} test_def;

/* ------------------------------------------------------- */

/* list of possible tests */
typedef enum
{
   test_is_running = 0,
   test_diosix_fork = 1,
   test_fp_addition = 2
} test_nr;

/* test functions */
kresult test__is_running(void);
kresult test__diosix_fork(void);
kresult test__fp_addition(void);

#endif
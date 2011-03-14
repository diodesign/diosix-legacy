/* kernel/ports/i386/debug.c
 * diosix portable run-time debug output for the kernel
 * Author : Chris Williams, printf code from http://my.execpc.com/~geezer/osd/ 
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

volatile unsigned int debug_spinlock = 0;

/* debug_writebyte
   Write a byte out to the debugging output stream, usually a serial port
   => c = character to write
      ptr = unused
*/
static int debug_writebyte(unsigned c, void **ptr)
{
   serial_writebyte(c); /* port-specific */
   return 0;
}

/* debug_stacktrace
   Dump a copy of the current kernel stack in a not-so-pretty way */
void debug_stacktrace(void)
{
#ifdef DEBUG
   lowlevel_stacktrace();
#endif
}

/* debug_assert
 Report an assert() failure and halt. Code taken from: 
 http://www.acm.uiuc.edu/sigops/roll_your_own/2.a.html */
void debug_assert(char *exp, char *file, char *basefile, unsigned int line)
{
   debug_printf("[debug] assert(%s) failed in file %s (included from %s), line %d\n",
                exp, file, basefile, line);
   debug_panic("assert() failed");
}

/* debug_panic
   Report a kernel panic, and either recover, restart or halt
   => str = message to display
*/
void debug_panic(const char *str)
{
   dprintf("*** PANIC: %s -- halting core %i\n", str, CPU_ID);
   
   lowlevel_disable_interrupts();
   while(1); /* go for halt */
}


/* strlen
   ANSI strlen() implementation
   => str = nul-terminated string to calculate length of
   <= number of characters
*/
unsigned int strlen(const unsigned char *str)
{
   unsigned int ret_val;

   for(ret_val = 0; *str != '\0'; str++)
      ret_val++;
   return ret_val;
}

/*------------------------------ dprintf code ----------------------------- */
/* ------------------------------------------------------------------------
    va_list set up for printf()
   ------------------------------------------------------------------------ */
typedef unsigned char *va_list;

/* width of stack == width of int */
#define   STACKITEM   int

/* round up width of objects pushed on stack. The expression before the
& ensures that we get 0 for objects of size 0. */
#define   VA_SIZE(TYPE)               \
   ((sizeof(TYPE) + sizeof(STACKITEM) - 1)   \
      & ~(sizeof(STACKITEM) - 1))

/* &(LASTARG) points to the LEFTMOST argument of the function call
(before the ...) */
#define   va_start(AP, LASTARG)   \
   (AP=((va_list)&(LASTARG) + VA_SIZE(LASTARG)))

#define va_end(AP)   /* nothing */

#define va_arg(AP, TYPE)   \
   (AP += VA_SIZE(TYPE), *((TYPE *)(AP - VA_SIZE(TYPE))))


/* ------------------------------------------------------------------------
    Write strings and information out to the user
   ------------------------------------------------------------------------ */

/* flags used in processing format string */
#define         PR_LJ   0x01    /* left justify */
#define         PR_CA   0x02    /* use A-F instead of a-f for hex */
#define         PR_SG   0x04    /* signed numeric conversion (%d vs. %u) */
#define         PR_32   0x08    /* long (32-bit) numeric conversion */
#define         PR_16   0x10    /* short (16-bit) numeric conversion */
#define         PR_WS   0x20    /* PR_SG set and num was < 0 */
#define         PR_LZ   0x40    /* pad left with '0' instead of ' ' */
#define         PR_FP   0x80    /* pointers are far */

/* largest number handled is 2^32-1, lowest radix handled is 8.
2^32-1 in base 8 has 11 digits (add 5 for trailing NUL and for slop) */
#define         PR_BUFLEN       16

typedef int (*fnptr_t)(unsigned c, void **helper);

/* debug_do_printf
   Minimal subfunction for ANSI C printf(): %[flag][width][.prec][mod][conv]
   => fmt = format string
      args = list of arguments to go with format string
      fn = function to call with arg 'ptr' for each character to be output
           ie: fn(character, ptr);
   <= total number of characters output
*/
static int debug_do_printf(const char *fmt, va_list args, fnptr_t fn, void *ptr)
{
   unsigned state, flags, radix, actual_wd, count, given_wd;
   unsigned char *where, buf[PR_BUFLEN];
   long num;

   state = flags = count = given_wd = 0;
   /* begin scanning format specifier list */
   for(; *fmt; fmt++)
   {
      switch(state)
      {
         /* STATE 0: AWAITING % */
         case 0:
            if(*fmt != '%')     /* not %... */
            {
               fn(*fmt, &ptr);  /* ...just echo it */
               count++;
               break;
            }
            /* found %, get next char and advance state to check if
               next char is a flag */
            state++;
            fmt++;
            /* FALL THROUGH */

         /* STATE 1: AWAITING FLAGS (%-0) */
         case 1:
            if(*fmt == '%')     /* %% */
            {
               fn(*fmt, &ptr);
               count++;
               state = flags = given_wd = 0;
               break;
            }
            if(*fmt == '-')
            {
               if(flags & PR_LJ)/* %-- is illegal */
                  state = flags = given_wd = 0;
               else
                  flags |= PR_LJ;
               break;
            }

            /* not a flag char: advance state to check if it's field width */
            state++;

            /* check now for '%0...' */
            if(*fmt == '0')
            {
               flags |= PR_LZ;
               fmt++;
            }
            /* FALL THROUGH */

         /* STATE 2: AWAITING (NUMERIC) FIELD WIDTH */
         case 2:
            if(*fmt >= '0' && *fmt <= '9')
            {
               given_wd = 10 * given_wd + (*fmt - '0');
               break;
            }

            /* not field width: advance state to check if it's a modifier */
            state++;

            /* FALL THROUGH */
         /* STATE 3: AWAITING MODIFIER CHARS (FNlh) */
         case 3:
            if(*fmt == 'F')
            {
               flags |= PR_FP;
               break;
            }
            if(*fmt == 'N') break;
            if(*fmt == 'l')
            {
               flags |= PR_32;
               break;
            }
            if(*fmt == 'h')
            {
               flags |= PR_16;
               break;
            }

            /* not modifier: advance state to check if it's a conversion char */
            state++;
            /* FALL THROUGH */

         /* STATE 4: AWAITING CONVERSION CHARS (Xxpndiuocs) */
         case 4:
            where = buf + PR_BUFLEN - 1;
            *where = '\0';
            switch(*fmt)
            {
               case 'X':
                  flags |= PR_CA;
                  /* FALL THROUGH */
               /* xxx - far pointers (%Fp, %Fn) not yet supported */
               case 'x':
               case 'p':
               case 'n':
                  radix = 16;
                  goto DO_NUM;
               case 'd':
               case 'i':
                  flags |= PR_SG;
                  /* FALL THROUGH */
               case 'u':
                  radix = 10;
                  goto DO_NUM;
               case 'o':
                  radix = 8;

/* load the value to be printed. l=long=32 bits: */
DO_NUM:          if(flags & PR_32)
                     num = va_arg(args, unsigned long);
                  /* h=short=16 bits (signed or unsigned) */
                  else
                     if(flags & PR_16)
                     {
                        if(flags & PR_SG)
                           num = va_arg(args, short);
                        else
                           num = va_arg(args, unsigned short);
                     }
                     /* no h nor l: sizeof(int) bits (signed or unsigned) */
                     else
                     {
                        if(flags & PR_SG)
                           num = va_arg(args, int);
                        else
                           num = va_arg(args, unsigned int);
                     }
                     /* take care of sign */
                     if(flags & PR_SG)
                     {
                        if(num < 0)
                        {
                           flags |= PR_WS;
                           num = -num;
                        }
                     }
   /* convert binary to octal/decimal/hex ASCII
   OK, I found my mistake. The math here is _always_ unsigned */
                     do
                     {
                        unsigned long temp;
          
                        temp = (unsigned long)num % radix;
                        where--;
                        if(temp < 10)
                           *where = temp + '0';
                        else
                           if(flags & PR_CA)
                              *where = temp - 10 + 'A';
                           else
                              *where = temp - 10 + 'a';
                        
                        switch(radix)
                        {
                           case 8:
                              num = (unsigned long)num >> 3;
                              break;
                              
                           case 16:
                              num = (unsigned long)num >> 4;
                              break;
                              
                           default:
                               num = (unsigned long)num / radix;
                        }
                     }
                     while(num != 0);
                     goto EMIT;

               case 'c':
/* disallow pad-left-with-zeroes for %c */
                  flags &= ~PR_LZ;
                  where--;
                  *where = (unsigned char)va_arg(args, unsigned char);
                  actual_wd = 1;
                  goto EMIT2;

               case 's':
/* disallow pad-left-with-zeroes for %s */
                  flags &= ~PR_LZ;
                  where = va_arg(args, unsigned char *);

EMIT:
                  actual_wd = strlen(where);
                  if(flags & PR_WS)
                     actual_wd++;
/* if we pad left with ZEROES, do the sign now */
                  if((flags & (PR_WS | PR_LZ)) == (PR_WS | PR_LZ))
                  {
                     fn('-', &ptr);
                     count++;
                  }
/* pad on left with spaces or zeroes (for right justify) */
EMIT2:            if((flags & PR_LJ) == 0)
                  {
                     while(given_wd > actual_wd)
                     {
                        fn(flags & PR_LZ ? '0' : ' ', &ptr);
                        count++;
                        given_wd--;
                     }
                  }
/* if we pad left with SPACES, do the sign now */
                  if((flags & (PR_WS | PR_LZ)) == PR_WS)
                  {
                     fn('-', &ptr);
                     count++;
                  }
/* emit string/char/converted number */
                  while(*where != '\0')
                  {
                     fn(*where++, &ptr);
                     count++;
                  }
/* pad on right with spaces (for left justify) */
                  if(given_wd < actual_wd)
                     given_wd = 0;
                  else
                     given_wd -= actual_wd;
                  for(; given_wd; given_wd--)
                  {
                     fn(' ', &ptr);
                     count++;
                  }
                  break;

               default:
                  break;
            }

            default:
               state = flags = given_wd = 0;
               break;
         }
      }

   return count;
}

/* debug_printf
   Output a formatted string to the user via the kernel's serial channel
   => as per ANSI C's printf()
   <= none
*/
void debug_printf(const char *fmt, ...)
{
   va_list args;

   /* gain exclusive access to the debug output stream */
   lock_spin(&debug_spinlock);
   
   va_start(args, fmt);
   (void)debug_do_printf(fmt, args, debug_writebyte, NULL);
   va_end(args);
   
   /* release the debug output stream */
   unlock_spin(&debug_spinlock);
}

/* keep a copy of the table pointer */
char *debug_sym_tbl_start = NULL;
char *debug_sym_tbl_end = NULL;

/* debug_init_sym_table
   Prepare a text-based symbol table for look up
   => table = pointer to symbol table file in memory
      end = pointer to the end of the symbol table in memory
   <= success or an error code
*/
kresult debug_init_sym_table(char *table, char *end)
{   
   debug_sym_tbl_start = table;
   debug_sym_tbl_end = end;

   return success;
}

/* debug_ascii2uint
   Convert a space-terminated ASCII string into a 32bit unsigned integer
   => str = pointer to word containing pointer to string to read
            this pointer is updated to point to the end of the string
      base = 10 for unsigned decimal text or 16 for unsigned hex text
   <= converted unsigned int
*/
unsigned int debug_ascii2uint(char **str, unsigned char base)
{
   unsigned int value = 0, power = 1;
   char *ptr = *str;
   char *start = *str;
   
   /* skip to the end of the string */
   while(*ptr != ' ')
      ptr++;

   /* update the pointer to the end of the string */
   *str = ptr;

   ptr--; /* reverse over the space terminator */
   
   /* now work our way back through the string... */
   while((unsigned int)ptr >= (unsigned int)start)
   {
      /* ...converting the digits */
      switch(*ptr)
      {
         case '0': value = value + (0 * power); break;
         case '1': value = value + (1 * power); break;
         case '2': value = value + (2 * power); break;
         case '3': value = value + (3 * power); break;
         case '4': value = value + (4 * power); break;
         case '5': value = value + (5 * power); break;
         case '6': value = value + (6 * power); break;
         case '7': value = value + (7 * power); break;
         case '8': value = value + (8 * power); break;
         case '9': value = value + (9 * power); break;
         /* uppercase hex codes */
         case 'A': value = value + (10 * power); break;
         case 'B': value = value + (11 * power); break;
         case 'C': value = value + (12 * power); break;
         case 'D': value = value + (13 * power); break;
         case 'E': value = value + (14 * power); break;
         case 'F': value = value + (15 * power); break;
         /* lowercase hex codes */
         case 'a': value = value + (10 * power); break;
         case 'b': value = value + (11 * power); break;
         case 'c': value = value + (12 * power); break;
         case 'd': value = value + (13 * power); break;
         case 'e': value = value + (14 * power); break;
         case 'f': value = value + (15 * power); break;
      }

      ptr--;
      power = power * base;
   }
   
   return value;
}

/* debug_lookup_symbol
   Look up a symbol from its address and copy its name into
   the given buffer
   => addr = address to query
      buffer = where to write the name string
      size = buffer size in bytes
      symbol = pointer to word to write in base address of found symbol
   <= success or a failure code
*/
kresult debug_lookup_symbol(unsigned int addr, char *buffer, unsigned int size, unsigned int *symbol)
{
   /* give up now if we don't have a symbol table */
   if(!debug_sym_tbl_start || !debug_sym_tbl_end) return e_not_found;
   
   char *ptr = debug_sym_tbl_start;
   unsigned int sym_addr, sym_size;
   
   KSYM_DEBUG("[debug:%i] looking up symbol for %x\n", CPU_ID, addr);
   
   /* attempt to parse a line in the symbol file - rather brute force */
   while(ptr < debug_sym_tbl_end)
   {
      /* convert values, remembering to jump over space seperator */
      sym_addr = debug_ascii2uint(&ptr, 16); ptr++;
      sym_size = debug_ascii2uint(&ptr, 10); ptr++;
      
      KSYM_DEBUG("[debug:%i] found a symbol for %x size %i\n", CPU_ID, sym_addr, sym_size);
      
      /* look for an address match */
      if(addr >= sym_addr && addr < (sym_addr + sym_size))
      {
         /* copy the symbol string into the buffer */
         unsigned int loop = 0;
         
         while(*ptr != '\n' && loop < (size - sizeof(char)))
         {
            buffer[loop] = *ptr;
            loop++;
            ptr++;
         }
         
         /* write in the sym base address, terminate the string and get out of here */
         *symbol = sym_addr;
         buffer[loop] = NULL;
         return success;
      }
      
      /* skip to the end of the line */
      while(*ptr != '\n') ptr++;
      ptr++; /* skip over the newline terminator */
   }
   
   /* fail if we're still here */
   return e_failure;
}

/* debug_initialise
   The kernel wants to do some low level debugging. By default, the i386 port
   uses the first IBM PC serial port. */
void debug_initialise(void)
{
#ifdef DEBUG
   serial_initialise(); /* port-specific */
   
   /* can I get a witness? */
   dprintf("[core] Now debugging to serial port.\n");
#endif
}

/* release/arm_versatilepb/mkpayload.c
 * Combine a number of binaries into a block of multiboot payloads
 * Author : Chris Williams
 * Date   : Sat,30 Apr 2011.01:37:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../kernel/core/include/multiboot.h"
#include "../../kernel/core/include/atag.h"

/* syntax: mkpayload -o output file1 file2 ... fileN
   Concatenate files 'file1'...'fileN' into a block of multiboot payload modules
   stored in the binary file 'output'. The output file should be easily loadable by
   a bootloader as a trivial initrd that the diosix microkernel can parse.
   The format of the multiboot block is defined by the mb_module_t structure:
 
   +----------------------------------------+
   | number of modules present              | 32bits wide
   +----------------------------------------+

   ...then at least one of these blocks...
 
   +----------------------------------------+
   | offset to first byte of module data    | 32bits wide
   +----------------------------------------+
   | offset to last byte of module data     | 32bits wide
   +----------------------------------------+
   | offset to first byte of comment string | 32bits wide
   +----------------------------------------+
   | must be 0x00000000                     | 32bits wide
   +----------------------------------------+
   | module comment string, null-terminated | 0 or more bytes
   +----------------------------------------+
   | module file data                       | 1 or more bytes
   +----------------------------------------+
 
   * Offsets are measured from the start of the file 
   * Module files must exist and must be at least 1 byte long
   * Comment field is always the given filename of the module with a prepended / character 
 
   To compile:
   gcc -o mkpayload mkpayload.c
*/ 

/* the first three arguments to the program are fixed
   so the fourth arg is the first input filename. this
   macro therefore converts the arg loop number into
   the module number, which starts at 0 */
#define FIRST_INPUT_ARG       (3)
#define ARG_TO_INDEX(a)       ((a) - FIRST_INPUT_ARG)

#define COPY_BUFFER_SIZE      (1 * 1024 * 1024)

int main(int argc, char **argv)
{
   unsigned int mod_loop;
   int output_fh;
   int *input_fh = NULL;
   mb_module_t **modules = NULL;
   struct stat st;
   unsigned int offset;
   unsigned char *copy_buffer = NULL;
   int result = EXIT_SUCCESS;
   
   payload_blob_header payload_header;
   
   /* sanity check the entry params */
   if(argc < (FIRST_INPUT_ARG + 1) || strcmp(argv[1], "-o"))
   {
      printf("usage:\t%s -o outputfile inputfile1 ... inputfileN\n", argv[0]);
      printf("\tGenerate a block of multiboot payloads from a list of files\n");
      return EXIT_FAILURE;
   }
   
   /* allocate a copy buffer */
   copy_buffer = malloc(COPY_BUFFER_SIZE);
   
   if(!copy_buffer)
   {
      printf("[-] could not allocate heap for internal copy buffer -- bailing out\n");
      return EXIT_FAILURE;
   }
   
   /* open output file for writing */
   output_fh = open(argv[2], O_CREAT | O_WRONLY, S_IRWXU);
   if(output_fh == -1)
   {
      printf("[-] could not create output file '%s' for writing -- bailing out\n", argv[2]);
      free(copy_buffer);
      return EXIT_FAILURE;
   }
   
   /* allocate enough slots in the module table and input filehandle table
      for all the requested files, although we'll skip over missing files later on */
   modules = malloc(sizeof(mb_module_t *) * ARG_TO_INDEX(argc));
   input_fh = malloc(sizeof(int) * ARG_TO_INDEX(argc));

   if(!modules || !input_fh)
   {
      printf("[-] could not allocate heap for internal structures -- bailing out\n");
      
      /* clean up */
      if(modules) free(modules);
      if(input_fh) free(input_fh);
      close(output_fh);
      free(copy_buffer);
      return EXIT_FAILURE;
   }
   
   /* zero the module table, a NULL entry means no module present */
   bzero(modules, sizeof(mb_module_t *) * ARG_TO_INDEX(argc));
   
   /* fill the filehandle table with -1 entries to indicate no file open */
   for(mod_loop = 0; mod_loop < ARG_TO_INDEX(argc); mod_loop++)
      input_fh[mod_loop] = -1;
   
   /* skip the offset counter past the initial header */
   offset = sizeof(payload_header);
   
   payload_header.present = 0;
   
   /* run through the given files to check they exist and build up
      the metadata to later save to disc */
   for(mod_loop = FIRST_INPUT_ARG; mod_loop < argc; mod_loop++)
   {
      input_fh[ARG_TO_INDEX(mod_loop)] = open(argv[mod_loop], O_RDONLY);
      
      /* check to see if the file exists for reading */
      if(input_fh[ARG_TO_INDEX(mod_loop)] != -1 &&
         fstat(input_fh[ARG_TO_INDEX(mod_loop)], &st) == 0)
      {
         modules[ARG_TO_INDEX(mod_loop)] = malloc(sizeof(mb_module_t));
         if(modules[ARG_TO_INDEX(mod_loop)])
         {
            mb_module_t *module = modules[ARG_TO_INDEX(mod_loop)];
            
            /* string immediately follows this header */
            module->string = offset + sizeof(mb_module_t);
            
            /* module data starts after the filename string, including the string's prepended '/' and NULL terminator */
            module->mod_start = module->string + strlen(argv[mod_loop]) + (2 * sizeof(char));
            module->mod_end = module->mod_start + st.st_size;
            module->reserved = 0;
            
            /* adjust the offset to point after the module data */
            offset = module->mod_end;
            
            payload_header.present++; /* keep track of the number of modules to write out */
         }
         else
            printf("[-] could not allocate %i bytes for module '%s' metadata -- skipping\n",
                   sizeof(mb_module_t), argv[mod_loop]);
      }
      else
         printf("[-] could not open module '%s' for reading -- skipping\n", argv[mod_loop]);
   }
   
   /* we've got all our pieces in place, time to write out to the output file, starting with
      the header */
   if(write(output_fh, &payload_header, sizeof(payload_header)) == -1)
   {
      printf("[-] failed to write header data to output file -- bailing out\n"); 
      result = EXIT_FAILURE;
      goto clean_up;
   }
      
   /* now run through the modules, writing them out to disc if present */
   for(mod_loop = FIRST_INPUT_ARG; mod_loop < argc; mod_loop++)
   {      
      if(modules[ARG_TO_INDEX(mod_loop)])
      {
         mb_module_t *module = modules[ARG_TO_INDEX(mod_loop)];
         int bytes_read = 0;
         
         /* write out the fields for the module */
         if(write(output_fh, module, sizeof(mb_module_t)) == -1)
         {
            printf("[-] failed to write module header data to output file -- bailing out\n"); 
            result = EXIT_FAILURE;
            goto clean_up;
         }
         
         /* write out the filename string inc null term */
         if(write(output_fh, "/", sizeof(char)) == -1 ||
            write(output_fh, argv[mod_loop], strlen(argv[mod_loop]) + sizeof(char)) == -1)
         {
            printf("[-] failed to write module string to output file -- bailing out\n"); 
            result = EXIT_FAILURE;
            goto clean_up;
         }
         
         /* copy data from input files into output file */
         do
         {            
            bytes_read = read(input_fh[ARG_TO_INDEX(mod_loop)], copy_buffer, COPY_BUFFER_SIZE);
            if(bytes_read > 0) write(output_fh, copy_buffer, bytes_read);
         }
         while(bytes_read > 0);
         
         if(bytes_read == -1)
         {
            printf("[-] failed to read file '%s' -- bailing out\n", argv[mod_loop]); 
            result = EXIT_FAILURE;
            goto clean_up;
         }
      }
   }
      
clean_up:
   /* close any files we have open, free any memory */
   for(mod_loop = 0; mod_loop < ARG_TO_INDEX(argc); mod_loop++)
   {
      if(modules[mod_loop]) free(modules[mod_loop]);
      
      if(input_fh[mod_loop] != -1)
         if(close(input_fh[mod_loop]) != 0)
            printf("[-] cleaning up for module %i failed!\n", mod_loop);
   }
   
   /* more clean up */
   close(output_fh);
   free(modules);
   free(input_fh);
   free(copy_buffer);
   
   return result; /* set to EXIT_SUCCESS unless a failure happened */
}




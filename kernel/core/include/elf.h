/* kernel/core/include/elf.h
 * structures to define ELF executables
 * Author : John-Mark Bell
 * Date   : Tue,03 Apr 2007.22:29:29

Lifted from code used under agreement copyright (c) John-Mark Bell <jmb202@ecs.soton.ac.uk>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef __ELF_H
#define __ELF_H

/* Header Data Types */
#define Elf32_Addr      unsigned int
#define Elf32_Half      unsigned short
#define Elf32_Off       unsigned int
#define Elf32_Sword     int
#define Elf32_Word      unsigned int

#define EI_NIDENT       (16)
#define ET_EXEC         (2)
#define EM_PORT_X86     (0x03)  /* for i386 */
#define EM_PORT_ARM     (0x28)  /* for ARM */
#define PT_LOAD         (1)

/* ELF File Header */
typedef struct {

  unsigned char e_ident[EI_NIDENT];
  Elf32_Half    e_type;
  Elf32_Half    e_machine;
  Elf32_Word    e_version;
  Elf32_Addr    e_entry;
  Elf32_Off     e_phoff;
  Elf32_Off     e_shoff;
  Elf32_Word    e_flags;
  Elf32_Half    e_ehsize;
  Elf32_Half    e_phentsize;
  Elf32_Half    e_phnum;
  Elf32_Half    e_shentsize;
  Elf32_Half    e_shnum;
  Elf32_Half    e_shstrndx;

} Elf32_Ehdr;

/* Program header */
typedef struct {

  Elf32_Word   p_type;
  Elf32_Off    p_offset;
  Elf32_Addr   p_vaddr;
  Elf32_Addr   p_paddr;
  Elf32_Word   p_filesz;
  Elf32_Word   p_memsz;
  Elf32_Word   p_flags;
  Elf32_Word   p_align;

} Elf32_Phdr;

#endif

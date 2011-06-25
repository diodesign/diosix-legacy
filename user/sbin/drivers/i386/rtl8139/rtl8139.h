/* user/sbin/drivers/i386/rtl8139/rtl8139.h
 * Defines and prototypes for the rtl8139 card driver
 * Author : Chris Williams
 * Date   : Fri,24 Jun 2011.23:41:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _RTL8139_H_
#define _RTL8139_H_ 1

/* ---------------------------------------------------------------
   Protocol notes:
   * The NIC driver initialises the card and grabs physical memory
     for receive and send buffers
   * It then announces itself to the network stack and waits for
     interrupts and requests
   * The stack requests to share the buffers one by one and the NIC
     confirms
   
   * When a frame needs to be sent, the stack sends a message to the
     driver to lock the buffer. On reply, it writes into the send
     buffer and sends a message to the driver to initiate the send
     and unlock the buffer.
   * The driver receives the signal and instructs the card to send
     the packet

   * When a frame is received, the driver sends the stack a message
     to say a packet has arrived and locks the buffer.
   * When the stack is finished with the buffer, it unlocks the buffer
     by sending back a message. this also marks the buffer as ready to
     reuse
 */

#define RTL8139_VENDORID      (0x10ec)
#define RTL8139_DEVICEID      (0x8139)

/* PCI registers */
#define RTL8139_PCI_IDR0      (0x00)
#define RTL8139_PCI_IOAR0     (0x10)
#define RTL8139_PCI_IOAR1     (0x12)

/* IO space registers */
#define RTL8139_REG_TSR(a)    (0x10 + (a) * 4)
#define RTL8139_REG_TSA(a)    (0x20 + (a) * 4)
#define RTL8139_REG_RBSTART   (0x30)
#define RTL8139_REG_IMR       (0x3C)
#define RTL8139_REG_CR        (0x37)
#define RTL8139_REG_CAPR      (0x38)
#define RTL8139_REG_ISR       (0x3e)
#define RTL8139_REG_TCR       (0x40)
#define RTL8139_REG_RCR       (0x44)
#define RTL8139_REG_CONFIG0   (0x50)
#define RTL8139_REG_CONFIG1   (0x51)
#define RTL8139_REG_BMCR      (0x62)
#define RTL8139_REG_BMSR      (0x64)

/* register flags */
#define RTL8139_REG_TSR_TOK          (1 << 15)
#define RTL8139_REG_TSR_OWN          (1 << 13)
#define RTL8139_REG_TSR_SIZEMASK     ((1 << 13) - 1)

#define RTL8139_REG_CR_RESET         (1 << 4)
#define RTL8139_REG_CR_TRANS_EN      (1 << 2)
#define RTL8139_REG_CR_RECVR_EN      (1 << 3)

#define RTL8139_REG_IMR_TOK          (1 << 2)
#define RTL8139_REG_IMR_ROK          (1 << 0)

#define RTL8139_REG_RCR_WRAP         (1 << 7)
#define RTL8139_REG_RCR_ACCEPT_ERROR (1 << 5)
#define RTL8139_REG_RCR_ACCEPT_RUNT  (1 << 4)
#define RTL8139_REG_RCR_ACCEPT_BROAD (1 << 3)
#define RTL8139_REG_RCR_ACCEPT_MCAST (1 << 2)
#define RTL8139_REG_RCR_ACCEPT_MATCH (1 << 1)
#define RTL8139_REG_RCR_ACCEPT_ALL   (1 << 0)

#define RTL8139_REG_ISR_RECVOK       (1 << 0)
#define RTL8139_REG_ISR_RECVERR      (1 << 1)
#define RTL8139_REG_ISR_SENDOK       (1 << 2)
#define RTL8139_REG_ISR_SENDERR      (1 << 3)

/* bit pattern 110 (dec 6) selects max DMA burst of 1024 bytes
   for rx and tx FIFOs, in bits 8-10 of tx and rx config regs */
#define RTL8139_REG_RXTX_MAXDMA_1024 (6 << 8)

/* ---------------------------------------------------------------
   low memory map for the NIC driver

   0x2000                              per-thread space
   ----------------------------------  -------------------------
   first driver thread's space         recv buffer 0 (3 pages)
   ----------------------------------  -------------------------
   guard page                          guard page 
   ----------------------------------  -------------------------
   second driver thread's space        recv buffer 1 (3 pages)
   ----------------------------------  -------------------------
   guard page                          guard page
   ----------------------------------  -------------------------
   ...and so on                        recv buffer N (3 pages)
   ----------------------------------  -------------------------
                                       guard page
                                       -------------------------
                                       send buffer 0 (1 page)
                                       -------------------------
                                       guard page
                                       -------------------------
                                       send buffer 1 (1 page)
                                       -------------------------
                                       guard page
                                       -------------------------
                                       send buffer N (1 page)
                                       -------------------------
 
   * guard pages are not mapped and thus will fault if touched
     by the driver.
*/

#define BUFFERS_BASE                 (0x2000)

/* size of each buffer in whole 4KB pages */
#define RECV_BUFFER_PAGES            (3)
#define SEND_BUFFER_PAGES            (1)

/* number of recv and send buffers supported */
#define RECV_BUFFERS                 (4)
#define SEND_BUFFERS                 (4)
#define RECV_BUFFERS_TOTAL           ((RECV_BUFFER_PAGES + 1) * RECV_BUFFERS)
#define SEND_BUFFERS_TOTAL           ((SEND_BUFFER_PAGES + 1) * SEND_BUFFERS)
#define PER_THREAD_BUFFERS_TOTAL     (RECV_BUFFERS_TOTAL + SEND_BUFFERS_TOTAL)

/* calculate the base address of the per-thread buffer space, a is the driver thread id (from 0) */
#define THREAD_BUFFERS_BASE(a)       ((a) * PER_THREAD_BUFFERS_TOTAL)

/* calculate the base address of the recv buffer for a given thread id (a) and buffer number (b) */
#define THREAD_RECV_BUFFER(a,b)      (THREAD_BUFFERS_BASE(a) + (RECV_BUFFER_PAGES + 1) * (b))
/* calculate the base address of the send buffer for a given thread id (a) and buffer number (b) */
#define THREAD_SEND_BUFFER(a,b)      (THREAD_RECV_BUFFER(a, (RECV_BUFFERS + 1)) + (SEND_BUFFER_PAGES + 1) * (b))

/* size of buffer to hold diosix messages */
#define MSG_RECEIVE_BUFFER_SIZE      (sizeof(diosix_nic_req))

/* define a structure holding a card's details */
typedef struct
{
   /* use to stop the interrupt and main driver threads from accessing them
      same structures at the same time */
   volatile unsigned char recv_lock, send_lock;
   
   unsigned char id; /* give each card its own id */
   
   /* define the card's hardware resources */
   unsigned short bus, slot, irq, iobase;

   /* keep a record of the card's MAC address */
   char mac[NIC_MAC_LENGTH];
   
   /* keep a record of where the physical addresses are */
   void *recv_phys[RECV_BUFFERS];
   void *send_phys[SEND_BUFFERS];
   
   /* keep a copy of the calculated virtual addresses */
   void *recv_virt[RECV_BUFFERS];
   void *send_virt[SEND_BUFFERS];
   
   /* each buffer has a lock bit (1 for locked, 0 for unlocked).
      these are stored as a bitmap. shift the number of buffers
      down 3 to calculate the number of bytes required to hold
      the bitmap */
   volatile unsigned char recv_locks[(RECV_BUFFERS >> 3) + 1];
   volatile unsigned char send_locks[(SEND_BUFFERS >> 3) + 1];
   
   /* when a buffer is filled with a new packet, set its bit */
   volatile unsigned char recv_filled[(RECV_BUFFERS >> 3) + 1];

   /* when a buffer has been filled and is awaiting sending to the line, set its bit */
   volatile unsigned char send_filled[(RECV_BUFFERS >> 3) + 1];
   
   /* these suggest the next available buffers to use and
      sets the buffer currently the receive one. */
   volatile unsigned char next_recv, next_send, current_recv;
   
   /* each card will need some space to communicate with 
      the rest of the system */
   diosix_msg_info msg;
   unsigned char msg_buffer[MSG_RECEIVE_BUFFER_SIZE];
} nic_info;

/* use this to define the name of the interface */
#define NIC_IF_NAME  "ert%i"

/* prototypes */
unsigned char set_buffer_inuse(volatile unsigned char *bitmap, unsigned char nr);
unsigned char clear_buffer_inuse(volatile unsigned char *bitmap, unsigned char nr);
kresult change_recv_buffer(nic_info *nic);

#endif

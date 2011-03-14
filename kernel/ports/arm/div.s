/* kernel/ports/arm/div.s
 * ARM-specific integer division routine
 * Author : Chris Williams
 * Date   : Sun,13 Mar 2011.21:56:00
 
 Cribbed from: http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0473c/CEGECDGD.html

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

.code 32
.balign 4

.global arm_udiv32

/* arm_udiv32 - perform an unsigned integer division
   => R0 = numerator, R1 = denominator
   <= R0 = result
*/
.func arm_udiv32
arm_udiv32:
STMFD   r13!, {r1, r2, r3}

MOV     r3, r1
CMP     r3, r0, LSR #1
double:
MOVLS   r3, r3, LSL #1
CMP     r3, r0, LSR #1
BLS     double

MOV     r2, #0

halve:
CMP     r0, r3
SUBCS   r0, r0, r3
ADC     r2, r2, r2
MOV     r3, r3, LSR #1
CMP     r3, r1
BHS     halve

MOV     r0, r2 
LDMFD   r13!, {r1, r2, r3}
MOV     pc, lr
.endfunc

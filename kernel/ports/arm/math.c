/* kernel/ports/arm/math.c
 * Math support code for non-FP machines (ARM)
 * Author : Chris Williams
 * Date   : Sun,12 Mar 2011.07:11.00

 From http://www.codesourcery.com/archives/arm-gnu/msg02379.html
 This code was suggested by Julian Brown from CodeSourcery. It is in public domain.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* define these compiler intrinsics that may be missing */
unsigned int __aeabi_uidivmod(unsigned int numerator, unsigned int denominator)
{   
   /* save us a bit of time if possible */
   if(!numerator) return 0;
   if(denominator == 1) return numerator;

   /* call the integer division ARM code */
   return arm_udiv32(numerator, denominator);
}

int __aeabi_idiv(int numerator, int denominator)
{      
   int neg_result = (numerator ^ denominator) & 0x80000000;
   int result = __aeabi_uidivmod((numerator < 0) ? -numerator : numerator, (denominator < 0) ? -denominator : denominator);
   return neg_result ? -result : result;
}

unsigned __aeabi_uidiv(unsigned int numerator, unsigned int denominator)
{   
   return __aeabi_uidivmod (numerator, denominator);
}

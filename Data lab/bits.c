/*
 * CS:APP Data Lab
 *
 * <Please put your name and userid here>
 *
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 */

/* Instructions to Students:

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:

  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code
  must conform to the following style:

  long Funct(long arg1, long arg2, ...) {
      // brief description of how your implementation works
      long var1 = Expr1;
      ...
      long varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. (Long) integer constants 0 through 255 (0xFFL), inclusive. You are
      not allowed to use big constants such as 0xffffffffL.
  3. Function arguments and local variables (no global variables).
  4. Local variables of type int and long
  5. Unary integer operations ! ~
     - Their arguments can have types int or long
     - Note that ! always returns int, even if the argument is long
  6. Binary integer operations & ^ | + << >>
     - Their arguments can have types int or long
  7. Casting from int to long and from long to int

  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting other than between int and long.
  7. Use any data type other than int or long.  This implies that you
     cannot use arrays, structs, or unions.

  You may assume that your machine:
  1. Uses 2s complement representations for int and long.
  2. Data type int is 32 bits, long is 64.
  3. Performs right shifts arithmetically.
  4. Has unpredictable behavior when shifting if the shift amount
     is less than 0 or greater than 31 (int) or 63 (long)

EXAMPLES OF ACCEPTABLE CODING STYLE:
  //
  // pow2plus1 - returns 2^x + 1, where 0 <= x <= 63
  //
  long pow2plus1(long x) {
     // exploit ability of shifts to compute powers of 2
     // Note that the 'L' indicates a long constant
     return (1L << x) + 1L;
  }

  //
  // pow2plus4 - returns 2^x + 4, where 0 <= x <= 63
  //
  long pow2plus4(long x) {
     // exploit ability of shifts to compute powers of 2
     long result = (1L << x);
     result += 4L;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implement floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants. You can use any
arithmetic, logical, or comparison operations on int or unsigned data.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to
     check the legality of your solutions.
  2. Each function has a maximum number of operations (integer, logical,
     or comparison) that you are allowed to use for your implementation
     of the function.  The max operator count is checked by dlc.
     Note that assignment ('=') is not counted; you may use as many of
     these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.
*/

/* CAUTION: Do not add an #include of <stdio.h> (or any other C
   library header) to this file.  C library headers almost always
   contain constructs that dlc does not understand.  For debugging,
   you can use printf, which is declared for you just below.  It is
   normally bad practice to declare C library functions by hand, but
   in this case it's less trouble than any alternative.

   CAUTION: You must remove all your debugging printf's again before
   submitting your code or testing it with dlc or the BDD checker.  */

extern int printf(const char *, ...);

/* Edit the functions below.  Good luck!  */
/* Bits */
/*
 * bitMatch - Create mask indicating which bits in x match those in y
 *            using only ~ and &
 *   Example: bitMatch(0x7L, 0xEL) = 0xFFFFFFFFFFFFFFF6L
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
long bitMatch(long x, long y) {
    long positive = x & y;
    long negative = ~x & ~y;
    return ~((~negative) & (~positive));
}
/*
 * leastBitPos - return a mask that marks the position of the
 *               least significant 1 bit. If x == 0, return 0
 *   Example: leastBitPos(96L) = 0x20L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
long leastBitPos(long x) {
    //
    long nex = ~x;
    return (nex + 1) & x;
}
/*
 * bitMask - Generate a mask consisting of all 1's
 *   between lowbit and highbit
 *   Examples: bitMask(5L,3L) = 0x38L
 *   Assume 0 <= lowbit < 64, and 0 <= highbit < 64
 *   If lowbit > highbit, then mask should be all 0's
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
long bitMask(long highbit, long lowbit) {
    long n = 1L << 63;
    long ifhi63 = !(highbit + ~62);
    long high = ~((n << ifhi63) >> (63 + ~highbit + ifhi63));
    long low = (n) >> (64 + ~lowbit);
    return (long)(high & low);
}
/*
 * isPalindrome - Return 1 if bit pattern in x is equal to its mirror image
 *   Example: isPalindrome(0x6F0F0123c480F0F6L) = 1L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 70
 *   Rating: 4
 */
long isPalindrome(long x) {
    long pn = x >> 63;
    long minus1 = ~0;
    long backhalf = 0;
    long fronthalf = 0;
    long back16 = 0;
    long front16 = 0;
    long reverse16 = 0;
    long back8 = 255;
    long front8 = 0;
    long reverse8 = 0;
    long back4 = 15;
    long front4 = 0;
    long reverse4 = 0;
    long back2 = 51;
    long front2 = 0;
    long reverse2 = 0;
    long back1 = 85;
    long front1 = 0;
    long reverse1 = 0;
    x = (x ^ pn);
    backhalf = (1L << 32) + minus1;
    fronthalf = (~backhalf);
    backhalf = backhalf & x;
    fronthalf = fronthalf & x;
    fronthalf = fronthalf >> 32;
    // reverse fronthalf;
    // 00000000000000001111111111111111
    back16 = (1L << 16) + minus1;
    front16 = ~back16;
    reverse16 = ((back16 & fronthalf) << 16) | ((front16 & fronthalf) >> 16);
    // 0000000011111111
    back8 = (back8 << 16) + back8;
    front8 = ~back8;
    reverse8 = ((reverse16 & back8) << 8) | ((reverse16 & front8) >> 8);
    // 00001111
    back4 = (back4 << 8) + back4;
    back4 = (back4 << 16) + back4;
    front4 = ~back4;
    reverse4 = ((reverse8 & back4) << 4) | ((reverse8 & front4) >> 4);
    // 0011001100110011
    back2 = (back2 << 8) + back2;
    back2 = (back2 << 16) + back2;
    front2 = ~back2;
    reverse2 = ((reverse4 & back2) << 2) | ((reverse4 & front2) >> 2);
    // 010101010101
    back1 = (back1 << 8) + back1;
    back1 = (back1 << 16) + back1;
    front1 = ~back1;
    reverse1 = ((reverse2 & back1) << 1) | ((reverse2 & front1) >> 1);
    return !(reverse1 ^ backhalf);
}
/* Arithmetic */
/*
 * isTmin - returns 1 if x is the minimum, two's complement number,
 *     and 0 otherwise
 *   Legal ops: ! ~ & ^ | +
 *   Max ops: 10
 *   Rating: 1
 */
long isTmin(long x) {
    long iszero = !x;
    long valid = !(x + x);

    return (valid & (!iszero));
}
/*
 * oddBits - return word with all odd-numbered bits set to 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 8
 *   Rating: 2
 */
long oddBits(void) {
    long y = 0;
    long z = 0;
    y = (85 << 8) | 85;
    z = (y << 16) | y;
    return ~(z << 32 | z);
}
/*
 * multFiveEighths - multiplies by 5/8 rounding toward 0.
 *   Should exactly duplicate effect of C expression (x*5/8),
 *   including overflow behavior.
 *   Examples:
 *     multFiveEighths(77L) = 48L
 *     multFiveEighths(-22L) = -13L
 *     multFiveEighths(2305843009213693952L) = -864691128455135232L (overflow)
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 3
 */
long multFiveEighths(long x) {
    long fourx = 0;
    long fivex = 0;
    long abs = 0;
    long absfivex = 0;
    fourx = x << 2;
    fivex = x + fourx;
    abs = fivex >> 63;
    absfivex = (fivex) + (abs & 7);

    return absfivex >> 3;
}
/*
 * leftBitCount - returns count of number of consective 1's in
 *     left-hand (most significant) end of word.
 *   Examples: leftBitCount(-1L) = 64L, leftBitCount(0xFFF0F0F000000000L) = 12L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 60
 *   Rating: 4
 */
long leftBitCount(long x) {
    // Binary Search
    long Tmin = 0;

    long thirtytwobit = 0;
    long ifthirtytwo = 0;
    long sixteenbit = 0;
    long ifsixteen = 0;
    long eightbit = 0;
    long ifeight = 0;
    long fourbit = 0;
    long iffour = 0;
    long twobit = 0;
    long iftwo = 0;
    long onebit = 0;
    long ifone = 0;
    long result = 0;

    Tmin = 1L << 63;
    thirtytwobit = Tmin >> 31;
    ifthirtytwo = !(thirtytwobit ^ (x & thirtytwobit));
    sixteenbit = thirtytwobit >> (ifthirtytwo << 4);
    sixteenbit = sixteenbit << ((!ifthirtytwo) << 4);
    ifsixteen = !(sixteenbit ^ (x & sixteenbit));
    // 8
    eightbit = sixteenbit >> (ifsixteen << 3);
    eightbit = eightbit << ((!ifsixteen) << 3);
    ifeight = !(eightbit ^ (x & eightbit));
    // 4
    fourbit = eightbit >> (ifeight << 2);
    fourbit = fourbit << ((!ifeight) << 2);
    iffour = !(fourbit ^ (x & fourbit));
    // 2
    twobit = fourbit >> (iffour << 1);
    twobit = twobit << ((!iffour) << 1);
    iftwo = !(twobit ^ (x & twobit));
    // 1
    onebit = twobit >> (iftwo);
    onebit = onebit << ((!iftwo));
    ifone = !(onebit ^ (x & onebit));
    // 1
    result = ((ifthirtytwo << 5) | (ifsixteen << 4) | (ifeight << 3) |
              (iffour << 2) | (iftwo << 1) | ifone) +
             (!(x + 1));

    return result;
}
/* Float */
/*
 * floatIsEqual - Compute f == g for floating point arguments f and g.
 *   Both the arguments are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representations of
 *   single-precision floating point values.
 *   If either argument is NaN, return 0.
 *   +0 and -0 are considered equal.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 25
 *   Rating: 2
 */
int floatIsEqual(unsigned uf, unsigned ug) {
    //
    int Tmin = 0;
    long last23 = 0;
    long exp = 0;
    Tmin = 1L << 31;
    // uf == +- 0 && ug == +- 0
    if (!((uf << 1) | (ug << 1)))
        return 1;
    // if last 52 is 0;
    last23 = ~(Tmin >> 8);
    exp = (Tmin >> 8) + Tmin;
    if ((!((exp & uf) ^ exp) && (last23 & uf)))
        return 0;

    return !(uf ^ ug);
}
/*
 * floatScale2 - Return bit-level equivalent of expression 2*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned floatScale2(unsigned uf) {
    // 分成normalize, denormalize, (Nan/infinite)
    int Tmin = 0;
    unsigned exp = 0;
    unsigned expm1 = 0;
    unsigned expodigit = 0;
    Tmin = 1L << 31;
    // NaN or 0 or +- Infinite
    expodigit = (Tmin >> 8) + Tmin;
    exp = expodigit & uf;
    if (!(expodigit ^ exp))
        return uf;
    // Denormalize Not 0
    if (!exp)
        return ((uf << 1) + (Tmin & uf));
    // Normalize overflow
    expm1 = (Tmin >> 7) + Tmin;
    if (!((expm1 & uf) ^ expm1))
        return (expodigit + (Tmin & uf));
    return uf + (1 << 23);
}

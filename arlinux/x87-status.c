/* Device diagnostic for FNSTSW without spilling virtual x87 operands.
 * Build as an i386 Windows console executable and run with Bionic Wine.
 * This checks arithmetic/state correctness, not game performance. */
#include <stdio.h>
#include <stdint.h>

static unsigned top(uint16_t word) { return (word >> 11) & 7; }

static int fast_stack(void)
{
    uint16_t a, b, c;
    double value;
    __asm__ volatile(
        "fninit\n\tfld1\n\tfld1\n\tfnstsw %%ax\n\tmovw %%ax,%0\n\t"
        "faddp\n\tfnstsw %1\n\tfstpl %3\n\tfnstsw %2"
        : "=m"(a), "=m"(b), "=m"(c), "=m"(value)
        : : "ax", "memory");
    return top(a) == 6 && top(b) == 7 && top(c) == 0 && value == 2.0;
}

static int incoming_stack(unsigned depth)
{
    uint16_t word;
    double value;
    __asm__ volatile("fninit" : : : "memory");
    /* Loop edges bring live x87 values from an earlier translated block. */
    for (unsigned i = 0; i < depth; ++i)
        __asm__ volatile("fld1" : : : "memory");
    __asm__ volatile("fnstsw %0" : "=m"(word) : : "memory");
    if (top(word) != ((8 - depth) & 7)) return 0;
    for (unsigned i = 1; i < depth; ++i)
        __asm__ volatile("faddp" : : : "memory");
    __asm__ volatile("fstpl %0\n\tfnstsw %%ax" : "=m"(value), "=a"(word) : : "memory");
    return value == (double)depth && top(word) == 0;
}

static int comparison(void)
{
    uint16_t less, equal, greater;
    double value;
    __asm__ volatile(
        "fninit\n\tfld1\n\tfldz\n\tfcom %%st(1)\n\tfnstsw %%ax\n\tmovw %%ax,%0\n\t"
        "fxch %%st(1)\n\tfcom %%st(1)\n\tfnstsw %2\n\t"
        "fcom %%st(0)\n\tfnstsw %1\n\tfaddp\n\tfstpl %3"
        : "=m"(less), "=m"(equal), "=m"(greater), "=m"(value)
        : : "ax", "memory");
    return (less & 0x4500) == 0x0100 && (equal & 0x4500) == 0x4000 &&
           (greater & 0x4500) == 0 && top(less) == 6 && value == 1.0;
}

int main(void)
{
    for (unsigned round = 0; round < 1000; ++round) {
        if (!fast_stack() || !comparison()) {
            printf("FAIL cached arithmetic/status round=%u\n", round);
            return 1;
        }
        for (unsigned depth = 1; depth <= 8; ++depth) {
            if (!incoming_stack(depth)) {
                printf("FAIL incoming stack round=%u depth=%u\n", round, depth);
                return 1;
            }
        }
    }
    puts("PASS FNSTSW register/memory, comparisons, cached operands, incoming stack and TOP wrap");
    return 0;
}

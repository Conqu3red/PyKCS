#include "bpf.h"

/* Computes a BiQuad filter on a sample */
smp_type BiQuad(smp_type sample, biquad *b)
{
    smp_type result;

    /* compute result */
    result = b->a0 * sample + b->a1 * b->x1 + b->a2 * b->x2 -
        b->a3 * b->y1 - b->a4 * b->y2;

    /* shift x1 to x2, sample to x1 */
    b->x2 = b->x1;
    b->x1 = sample;

    /* shift y1 to y2, result to y1 */
    b->y2 = b->y1;
    b->y1 = result;

    return result;
}

/* sets up a BiQuad Band-Pass Filter */
biquad *BiQuad_new_BPF(smp_type freq,
smp_type srate, smp_type bandwidth)
{
    biquad *b;
    smp_type A, omega, sn, cs, alpha, beta;
    smp_type a0, a1, a2, b0, b1, b2;

    b = malloc(sizeof(biquad));
    if (b == NULL)
        return NULL;

    /* setup variables */
    omega = 2 * M_PI * freq / srate;
    sn = sin(omega);
    cs = cos(omega);
    alpha = sn * sinh(M_LN2 / 2 * bandwidth * omega / sn);

    // BPF:
    b0 = alpha;
    b1 = 0;
    b2 = -alpha;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;

    /* precompute the coefficients */
    b->a0 = b0 / a0;
    b->a1 = b1 / a0;
    b->a2 = b2 / a0;
    b->a3 = a1 / a0;
    b->a4 = a2 / a0;

    /* zero initial samples */
    b->x1 = b->x2 = 0;
    b->y1 = b->y2 = 0;

    return b;
}
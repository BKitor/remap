#ifndef KERNEL_H
#define KERNEL_H

/* Function: vecAdd
 * ------------------------------------------------------------
 * A blocking host function that will calculate a vector
 * addition (A = A + B) within the Open MPI library.
 *
 * In/Output:
 *  a: A device pointer of vector A
 *
 * Inputs:
 *  b: A device pointer of vector B
 *  n: The number of elements in vector A and B. It is
 *     predicted that n shall not be much larger than
 *     16777216 = (16 * 1024 * 1024).
 *
 * returns: nothing
 *
 * ----------------------------------------------------------*/

#ifdef __cplusplus
extern "C"
{
#endif
  void vecAdd(float *a, float *b, int n);
#ifdef __cplusplus
}
#endif

#ifdef __CUDACC__
/* Function: vecAddImpl
 * ------------------------------------------------------------
 * The kernel function which implements the vector addition.
 * The actual function signature can change, if required, this
 * is only an example.
 * ----------------------------------------------------------*/

__global__ void vecAddImpl(float *a, float *b, int n);

#endif

#endif // KERNEL_H

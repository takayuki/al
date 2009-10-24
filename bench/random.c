#include <stdlib.h>

unsigned long
Random(unsigned long* next)
{
  return (*next = *next * 1103515245 + 12345) % ((unsigned long)RAND_MAX + 1);
}

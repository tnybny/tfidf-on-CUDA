#ifndef _PARSER_KERNEL_H_
#define _PARSER_KERNEL_H_

#include <stdio.h>
#include <string.h>
#include "hash_funcs.cpp"
#include "defs.h"
#include "hash_funcs.h"
#include "cuda_k.h"


/* This is only OK for small number of documents
   It returns the position of each entry in sorted pattern.
   On the host, extra work needs to be done to search for intended position. 
   TODO make it faster for large number of documents
   */

#endif

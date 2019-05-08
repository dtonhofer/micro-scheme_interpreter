#ifndef PARSER_H
#define PARSER_H

#include <stdlib.h>
#include <stdio.h>
#include "memory.h"

#define RINGSIZE    64   /* Size of ringbuffer */

typedef enum {OK,STOP,TERM,ERROR,BACK} status;

typedef struct RINGBUF *ringbuffer;

/* Exported procedures */

extern ringbuffer new_ringbuffer(FILE *stream);
extern void       release_ringbuffer(ringbuffer rb);
extern ipointer   read_call(ringbuffer rb,status *res);

#endif

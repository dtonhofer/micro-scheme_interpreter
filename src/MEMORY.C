/* ===========================================================================
   Memory Management
   -----------------
   We only allocate a single memory block. This block is divided into three
   parts:
   - Storage space for cons-boxes. Only cons-boxes may be found here.
   - Storage space for data. Only data (and therefore no pointer) may be
     found in that area.
   - Stack space. The space grows downward (toward the lower adresses) and uses
     predecrement/postincrement adressing.

   We assume that a machine pointer is the same size as a C long integer,
   and has got at least 32 bits; an integer has at least 16 bits. Further,
   a linear memory model is perequisite; avoid brain-dead Intel chips!

   Array of unsigned longints...
   +---------------------+-----------------------+----------------------+---+
   | cons-box storage    | data storage          |           <<   stack |   |
   +---------------------+-----------------------+----------------------+---+
   cbsl    <CBSLD>       dsl     <DSLD>          stack    <STACKD>
   base                  base                    base

   The size of the cons-box storage is CBSL longints.
   The data storage is DSL-CBSL longints big.
   The stack has got MEML-DSL longints.

   An additional, small-size stack (the "reverse stack") has been added at the
   top of memory. Only an operation to push() values on that stack exists. The
   purpose of this stack is to contain pointers to structures that do not
   change during program execution but which must be found during the mark
   phase of the garbage collector.

   Memory may be considered an array of unsigned longints.

   Cons-box structure
   ------------------
   Cons boxes consist of two pointers, put end to end. A consbox therefore has
   the size of a double longint (or double longword), i.e. a quadword.
   The car is located on the low longint, the cdr on the high longint.

   +--------+--------+
   | car    | cdr    |
   +--------+--------+
   0        4        8

   To get an integer number of cons-boxes into the cons-box storage, CBSL has
   to be even.

   The car and cdr contain machine adress pointers (of type unsigned long *).
   These always point to quadword-aligned, i.e. double-longword-aligned
   adresses. The three lowermost bits (at least) are therefore always 0,
   and are used for special purposes by the memory manager:

   Bit 0: "Marked bit"    : Used by the mark & sweep garbage collector.
   Bit 1: "Special bit 1"
   Bit 2: "Special bit 2"

   Both the car and the cdr have got these three bits.

   Pointer peculiarities
   ---------------------
   Pointers (car,cdr or register contents or stack contents) may be real
   pointers or otherwise "special values".

   A "special value" does not point to a storage place but contains the value
   itself. The special bits of such an element have been set to ZAP_SPECIAL;
   a special value can be obtained normally using "car" or "cdr". The magic
   module contains the handling of "special values".

   Cons-box peculiarities
   ----------------------
   The special bits of the cdr of a consbox may have been set to ENV_SPECIAL.
   This means that the cons-box is the header of an environment structure; the
   cdr of this same cons-box points to the environment elements; the car to
   the parent environment.

   The special bits of the cdr of a consbox may have been set to PROC_SPECIAL.
   This means that the cons-box stands for a procedure; the car pointing to
   a procedure-text, the cdr to some environment.

   Free list
   ---------
   Non-allocated cons-boxes are stored in a linked list. The cdr of the
   cons-box simply points to the next free one, or is NULL if no box is left.

   The GC sets (and resets) Bit 0 during the mark phase.

   Storage-box structure
   ---------------------
   A storage box may be of variable size, as it could store anything from
   characters to strings. To simplify things, we require the size of the
   storage box to be an even number of longints.
   A pointer to a storage box is always a pointer to the quadword-aligned
   first word, which contains only administrative data.

   Free storage boxes also will be stored in a linked list.

   Storage box structure is as follows:

   First longword (longword 0):

   Bit 0:     used by the mark & sweep garbage collector
   Bit 1-15:  15 bit typedescriptor
   Bit 16-31: size block, ranging from 0 (meaning 65536 longwords)
              to 65535 longwords.
              Block size includes this first longword. Moreover, as
              storage blocks must be an integer number of quadwords
              big, odd numbers are not allowed here.

   Second longword

   Stores data if the block is allocated
   Stores pointer to next free block otherwise

   Two stacks
   ----------
   The stack used by the scheme machine contains to type of values: Pointers
   to data structures (i.e. longints) and labels (small integers). It seems
   logical to use two stacks to separate these informations. The stack in
   the memory area only contains pointers; the label stack is implemented
   as an array of unsigned characters, which gives 256 different labels;
   this is amply sufficient.

   Garbage collection
   ------------------
   We use a non-recursive mark algorithm. As the only pointers may be found
   within the cons-box area, no special provision has to be made to allow the
   mark algorithm to traverse the storage area. Furthermore, we would like the
   garbage collector to be able to start up anytime. This means that we have
   to know about interesting pointers.

   Interesting pointers include:

   - Pointers on the pointer stack; all have to be checked.
   - Local variables containing pointers; the GC doesn't know about these.
     We one may do is
     -- Push all local pointers onto the stack before calling the GC.
        The GC may be called explicitely or automatically upon allocation
        of a cons-box or a storage area.
    -- Not use local variables.
   - The GC also has to know about the scheme machine registers; if these
     contain NIL values, the pointer value is discarded; otherwise the mark
     algorithm is called.

   Machine registers
   -----------------
   The scheme machine registers have to be visible to the garbage collector.
   That's why they have been declared in memory.h, and are initialized in
   init_mem().

=========================================================================== */

/*{{{  includes --*/
#include <stdio.h>
#include <stdlib.h>
#define NDEBUG
#include <assert.h>
#include "main.h"
#include "memory.h"
#include "help.h"     /* even_p() */
/*}}}  */

#define DEBUGMEM              /* debugging on */
#undef  DEBUGMEM

/*{{{  module global variables --*/
static ipointer  cbslbase;      /* cbox-base pointer     */
static ipointer  stackbase;     /* stack-base pointer    */
static ipointer  dslbase;       /* dsl-base pointer      */
static cpointer  lstackbase;    /* lstack-base pointer   */
static ipointer  cbox_free;     /* Consbox free list     */
static ipointer  stor_free;     /* Storage free list     */
static cpointer  lstack_ptr;    /* Label stack pointer   */
static ipointer  stack_ptr;     /* Stack-Pointer         */
static ipointer  revstack_ptr;  /* Reverse-stack-pointer */
static bool      cbsl_leaked;   /* If a long was lost    */
static bool      dsl_leaked;    /* If a long was lost    */
/*}}}  */

/*{{{  scheme machine registers --*/
ipointer val_reg;     /* Pointer to evaluation result */
ipointer env_reg;     /* Pointer to current environment */
ipointer fun_reg;     /* Pointer to procedure to be applied */
ipointer argl_reg;    /* Pointer to list of arguments */
ipointer exp_reg;     /* Pointer to expression to evaluate */
ipointer unev_reg;    /* Escape register */
uchar    cont_reg;    /* Jump-label */
/*}}}  */

/*{{{  memory configuration --*/
const ulong CBSLD     = 16382;   /* longs for cboxes    */
const ulong DSLD      = 16382;   /* longs for storage   */
const ulong STACKD    = 10240;   /* longs for stack     */
const ulong REVSTACKD = 2;       /* longs for the reverse-stack  */
const ulong LSTACKD   = 10240;   /* size of label stack */
/*}}}  */

/*{{{  constants for "zap-special" bits --*/
static const uint NO_SPECIAL   = 0;
static const uint ENV_SPECIAL  = 1;
static const uint PROC_SPECIAL = 2;
static const uint ZAP_SPECIAL  = 3;
/*}}}  */

/*{{{  procedure headers --*/
static  bool     quadword_aligned_p(ipointer ptr);
static  bool     car_unmarked_p(ipointer cur);
static  void     set_car_mark(ipointer cur);
static  void     unset_car_mark(ipointer cur);
static  bool     cdr_unmarked_p(ipointer cur);
static  void     set_cdr_mark(ipointer cur);
static  void     unset_cdr_mark(ipointer cur);
static  bool     storage_unmarked_p(ipointer cur);
static  void     set_storage_mark(ipointer cur);
static  void     unset_storage_mark(ipointer cur);
static  void     set_freeptr(ipointer this,ipointer that);
static  ipointer get_freeptr(ipointer this);
static  void     set_car_nomodify(ipointer this,ipointer that);
static  void     set_cdr_nomodify(ipointer this,ipointer that);
static  void     set_size(ipointer cur,ulong size);
static  ulong    get_size(ipointer cur);
static  void     sweep_cbox(void);
static  void     sweep_storage(void);
static  void     mark(ipointer cur);
/*}}}  */

/* ======================================================================== */
/* Initialization routine; to be called on startup                          */
/* ======================================================================== */

/*{{{  initialize managed memory --*/
void init_mem(void) {
   ulong    bigblocks;     /* Number of blocs of size 65536 */
   ulong    restblock;     /* Size of remaining block       */
   ipointer pointer;
   ulong    test1,test2,test3,test4;
   /* Alignment check: all limits must be quadword-aligned */
   if (!even_p(CBSLD) || !even_p(DSLD)) {
      printf("STARTUP-ERROR: invalid memory configuration at compile time.\n");
      exit(0);
   }
   /* Check if assumptions about machine architecture are correct */
   if (sizeof(pointer)!=sizeof(ulong) || sizeof(ulong)<4 || sizeof(uint)<2) {
      printf("STARTUP-ERROR: machine architecture is wrong.\n");
      exit(0);
   }
   /* Let's allocate */
   test1=((ulong)sizeof(uchar)*(ulong)LSTACKD);
   test2=((ulong)sizeof(ulong)*(ulong)(CBSLD+1));
   test3=((ulong)sizeof(ulong)*(ulong)(DSLD+1));
   test4=((ulong)sizeof(ulong)*(ulong)(STACKD+REVSTACKD));
   if (test1!=(ulong)(size_t)test1 || test2!=(ulong)(size_t)test2 ||
       test3!=(ulong)(size_t)test3 || test4!=(ulong)(size_t)test4) {
      printf("STARTUP-ERROR: request for too much memory, losing digits.\n");
      exit(0);
   }
   else {
      lstackbase=(cpointer)malloc((size_t)test1);
      stackbase =(ipointer)malloc((size_t)test4);
      cbslbase  =(ipointer)malloc((size_t)test2);
      dslbase   =(ipointer)malloc((size_t)test3);
   }
   if (stackbase==NULL || cbslbase==NULL || dslbase==NULL || lstackbase==NULL) {
     printf("STARTUP-ERROR: couldn't malloc() the requested memory.\n");
     exit(0);
   }
   else {
      cbsl_leaked=FALSE;
      dsl_leaked =FALSE;
      if (!quadword_aligned_p(cbslbase)) {
         cbslbase++;cbsl_leaked=TRUE;
      }
      if (!quadword_aligned_p(dslbase)) {
         dslbase++;dsl_leaked=TRUE;
      }
   }
   /* Initialize cons-box storage area */
   cbox_free=NIL;
   pointer=cbslbase;
   while ((ulong)pointer<(ulong)(cbslbase+CBSLD)) {
      set_car(pointer,NIL);
      set_cdr(pointer,cbox_free);
      cbox_free=pointer;
      pointer=pointer+2;
   }
   /* Initialize storage blocks storage area */
   bigblocks=DSLD/65536L;
   restblock=DSLD-(bigblocks*65536L);
   pointer=dslbase;
   stor_free=NIL;
   while (bigblocks>0) {
      *pointer=0L;
      set_freeptr(pointer,stor_free);
      set_size(pointer,65536L);
      stor_free=pointer;
      pointer=pointer+65536L;
      bigblocks--;
   }
   if (restblock!=0) {
      *pointer=0L;
      set_size(pointer,restblock);
      set_freeptr(pointer,stor_free);
      stor_free=pointer;
   }
   revstack_ptr=stackbase+STACKD;
   init_stack();
   init_registers();
}
/*}}}  */

/*{{{  reset both stacks --*/
void init_stack(void) {
   stack_ptr=stackbase+STACKD;
   lstack_ptr=lstackbase+LSTACKD;
}
/*}}}  */

/*{{{  initialize machine registers --*/
void init_registers(void) {
   val_reg=NIL;
   env_reg=NIL;
   fun_reg=NIL;
   argl_reg=NIL;
   exp_reg=NIL;
   unev_reg=NIL;
   cont_reg=0;
}
/*}}}  */

/*{{{  free allocated memory --*/
void cleanup_mem(void) {
   free((void *)lstackbase);
   free((void *)stackbase);
   if (dsl_leaked) free((void *)(dslbase-1)); else free((void *)dslbase);
   if (cbsl_leaked) free((void *)(cbslbase-1)); else free((void *)cbslbase);
}
/*}}}  */

/* ======================================================================== */
/* Getting statistical information                                          */
/* ======================================================================== */

/*{{{  number of free consboxes --*/
ulong stat_cbox_free(void) {
   ulong i;
   ipointer pointer;
   i=0;pointer=cbox_free;
   while (pointer!=NIL) {
      assert(cbox_p(pointer));
      pointer=cdr(pointer);
      i++;
   }
   return i;
}
/*}}}  */

/*{{{  number of free longints in storage --*/
ulong stat_storage_free(void) {
   ipointer pointer;
   ulong    i;
   pointer=stor_free;i=0;
   while (pointer!=NIL) {
      assert(storage_p(pointer));
      i=i+get_size(pointer)-1;
      pointer=get_freeptr(pointer);
   }
   return i;
}
/*}}}  */

/*{{{  number of blocs in storage --*/
ulong stat_storage_blocs(void) {
   ipointer pointer;
   ulong    i;
   pointer=stor_free;i=0;
   while (pointer!=NIL) {
      assert(storage_p(pointer));
      pointer=get_freeptr(pointer);
      i++;
   }
   return i;
}
/*}}}  */

/*{{{  number of free places in stack --*/
ulong stat_stack_free(void) {
   return (ulong)STACKD-(ulong)((stackbase+STACKD)-stack_ptr);
}
/*}}}  */

/*{{{  number of free places in lstack --*/
ulong stat_lstack_free(void) {
   return (ulong)LSTACKD-(ulong)((lstackbase+LSTACKD)-lstack_ptr);
}
/*}}}  */

/*{{{  print statistics to stdout --*/
void statistics_mem(void) {
   printf("\n  Free cons-boxes          :%8lu ",stat_cbox_free());
   printf("(start at 0x%lX).\n",(ulong)cbslbase);
   printf("  Free longints in storage :%8lu in %lu blocks ",
           stat_storage_free(),stat_storage_blocs());
   printf("(start at 0x%lX).\n",(ulong)dslbase);
   printf("  Free longints in stack   :%8lu ",stat_stack_free());
   printf("(start at 0x%lX).\n",(ulong)stackbase);
   printf("  Free places in lstack    :%8lu\n\n",stat_lstack_free());
}
/*}}}  */

/* ======================================================================== */
/* Allocation procedure; size is the size in bytes (!)                      */
/* ======================================================================== */

/*{{{  allocation of new consbox --*/
ipointer new_cons(void) {
   ipointer tmp;
   if (cbox_free==NIL) {
      garbage_collect();
      if (cbox_free==NIL) {
         printf("*** Out of cons box space ***\n");
         goto_recoverable_error();
      }
   }
   tmp=cbox_free;
   cbox_free=cdr(tmp);
   set_cdr(tmp,NIL); /* car is NIL already */
   return tmp;
}
/*}}}  */

/*{{{  allocation of new storage --*/
ipointer new_storage(ulong size) {
   ipointer tmp,last;
   ulong    restsize;
   /* First-fit */
   size=((((size+(sizeof(ulong))-1)/sizeof(ulong))+2)/2)*2;
   if (size>65536L) {
      printf("PROGRAM INTERNAL: memory.c: too large a block requested.\n");
      goto_recoverable_error();
   }
   /* Size is now number of longints, and even */
   tmp=stor_free;
   last=NIL;
   while (tmp!=NIL && get_size(tmp)<size) {
      assert(even_p(get_size(tmp)));
      last=tmp;
      tmp=get_freeptr(tmp);
   }
   if (tmp==NIL) {
      garbage_collect();
      tmp=stor_free;
      last=NIL;
      while (tmp!=NIL && get_size(tmp)<size) {
         assert(even_p(get_size(tmp)));
         last=tmp;
         tmp=get_freeptr(tmp);
      }
      if (tmp==NIL) {
         printf("*** Out of storage space ***\n");
         goto_recoverable_error();
      }
   }
   /* Could be we have to divide up the block */
   restsize=get_size(tmp)-size;
   if (restsize!=0) {
      /* Cut up */
      set_size(tmp,restsize);
      tmp=tmp+restsize;*tmp=0L;
      set_size(tmp,size);
   }
   else if (last==NIL) {
      stor_free=get_freeptr(tmp);
   }
   else {
      set_freeptr(last,get_freeptr(tmp));
   }
   return tmp;
}
/*}}}  */

/* ======================================================================== */
/* Stack procedures                                                         */
/* ======================================================================== */

/*{{{  push label onto label stack --*/
void push_label(uchar label) {
   lstack_ptr--;
   if ((ulong)lstack_ptr<(ulong)lstackbase) {
      printf("*** Label-stack overflow ***\n");
      goto_recoverable_error();
   }
   *lstack_ptr=label;
}
/*}}}  */

/*{{{  get label from label stack --*/
uchar pop_label(void) {
   uchar label;
   if ((ulong)lstack_ptr==(ulong)(lstackbase+LSTACKD)) {
      printf("PROGRAM ERROR: pop of empty label stack attempted.\n");
      goto_recoverable_error();
   }
   label=*lstack_ptr;
   lstack_ptr++;
   return label;
}
/*}}}  */

/*{{{  push pointer onto pointer stack --*/
void push_pointer(ipointer ptr) {
   stack_ptr--;
   if ((ulong)stack_ptr<(ulong)stackbase) {
      printf("*** Pointer-stack overflow ***\n");
      goto_recoverable_error();
   }
   *stack_ptr=(ulong)ptr;
}
/*}}}  */

/*{{{  get pointer from pointer stack --*/
ipointer pop_pointer(void) {
   ulong ptr;
   if ((ulong)stack_ptr==(ulong)(stackbase+STACKD)) {
      printf("PROGRAM ERROR: pop of empty pointer stack attempted.\n");
      goto_recoverable_error();
   }
   ptr=*stack_ptr;
   stack_ptr++;
   return (ipointer)ptr;
}
/*}}}  */

/*{{{  push pointer onto reverse stack --*/
void revpush_pointer(ipointer ptr) {
   if ((ulong)revstack_ptr>=(ulong)(stackbase+STACKD+REVSTACKD)) {
      printf("PROGRAM INTERNAL: reverse stack too small.\n");
      exit(0);
   }
   *revstack_ptr=(ulong)ptr;
   revstack_ptr=revstack_ptr+1;
}
/*}}}  */

/* ======================================================================== */
/* Other procedures                                                         */
/* ======================================================================== */

/*{{{  alignment check --*/
static bool quadword_aligned_p(ipointer ptr) {
   /* Varies with size of long int */
   return (((ulong)ptr & (ulong)(2*sizeof(ulong)-1)) == 0);
}
/*}}}  */

/*{{{  check if pointer is a "special value" --*/
bool special_p(ipointer cur) {
   return ((ulong)(cur) & 0x06L)!=0;
}
/*}}}  */

/*{{{  check if pointer points to storage --*/
bool storage_p(ipointer cur) {
   return (!special_p(cur) &&
           quadword_aligned_p(cur) &&
           (ulong)dslbase<=(ulong)cur &&
           (ulong)cur<(ulong)(dslbase+DSLD));
}
/*}}}  */

/*{{{  check if pointer points to consbox --*/
bool cbox_p(ipointer cur) {
   return (!special_p(cur) &&
           quadword_aligned_p(cur) &&
           (ulong)cbslbase<=(ulong)cur &&
           (ulong)cur<(ulong)(cbslbase+CBSLD));
}
/*}}}  */

/*{{{  check if car unmarked --*/
static bool car_unmarked_p(ipointer cur) {
   assert(cbox_p(cur));return (*cur & 0x01L)==0;
}
/*}}}  */

/*{{{  set the car's mark --*/
static void set_car_mark(ipointer cur) {
   assert(cbox_p(cur));*cur=(*cur | 0x01L);
}
/*}}}  */

/*{{{  reset the car's mark --*/
static void unset_car_mark(ipointer cur) {
   assert(cbox_p(cur));*cur=(*cur & ~0x01L);
}
/*}}}  */

/*{{{  check if cdr unmarked --*/
static bool cdr_unmarked_p(ipointer cur) {
   assert(cbox_p(cur));return (*(cur+1) & 0x01L)==0;
}
/*}}}  */

/*{{{  set the cdr's mark --*/
static void set_cdr_mark(ipointer cur) {
   assert(cbox_p(cur));*(cur+1)=(*(cur+1) | 0x01L);
}
/*}}}  */

/*{{{  reset the cdr's mark --*/
static void unset_cdr_mark(ipointer cur) {
   assert(cbox_p(cur));*(cur+1)=(*(cur+1) & ~0x01L);
}
/*}}}  */

/*{{{  check if storage unmarked --*/
static bool storage_unmarked_p(ipointer cur) {
   assert(storage_p(cur));return (*cur & 0x01L)==0;
}
/*}}}  */

/*{{{  set the storage's mark --*/
static void set_storage_mark(ipointer cur) {
   assert(storage_p(cur));*cur=(*cur | 0x01L);
}
/*}}}  */

/*{{{  reset the storage's mark --*/
static void unset_storage_mark(ipointer cur) {
   assert(storage_p(cur));*cur=(*cur & ~0x01L);
}
/*}}}  */

/*{{{  setting a pointer to be "special value" --*/
ipointer set_zap_special(ipointer cur) {
   return (ipointer)(((ulong)cur & ~0x06L) | ((ZAP_SPECIAL<<1) & 0x06L));
}
/*}}}  */

/*{{{  setting a cbox procedure hint --*/
void set_hint_procedure(ipointer cur) {
   assert(cbox_p(cur));
   *(cur+1)=(*(cur+1) & ~0x06L) | ((PROC_SPECIAL<<1) & 0x06L);
}
/*}}}  */

/*{{{  setting a cbox environment hint --*/
void set_hint_environment(ipointer cur) {
   assert(cbox_p(cur));
   *(cur+1)=(*(cur+1) & ~0x06L) | ((ENV_SPECIAL<<1) & 0x06L);
}
/*}}}  */

/*{{{  check if cbox is an environment header --*/
bool hint_environment_p(ipointer cur) {
   assert(cbox_p(cur));
   return (*(cur+1) & 0x06L)==((ENV_SPECIAL<<1) & 0x06L);
}
/*}}}  */

/*{{{  check if cbox is a procedure --*/
bool hint_procedure_p(ipointer cur) {
   assert(cbox_p(cur));
   return (*(cur+1) & 0x06L)==((PROC_SPECIAL<<1) & 0x06L);
}
/*}}}  */

/*{{{  get car of cbox, letting zap_special bits pass --*/
/* the mark bit has to be filtered such that the GC may use it */
ipointer car(ipointer cur) {
   assert(cbox_p(cur));
   if ((*cur & 0x06L)==(ulong)ZAP_SPECIAL<<1) {
      return (ipointer)(*cur & ~0x01L);
   }
   else {
      return (ipointer)(*cur & ~0x07L);
   }
}
/*}}}  */

/*{{{  get cdr of cbox, letting zap_special bits pass --*/
ipointer cdr(ipointer cur) {
   assert(cbox_p(cur));
   if ((*(cur+1) & 0x06L)==(ulong)ZAP_SPECIAL<<1) {
      return (ipointer)(*(cur+1) & ~0x01L);
   }
   else {
      return (ipointer)(*(cur+1) & ~0x07L);
   }
}
/*}}}  */

/*{{{  set car of cbox, modifying the lowermost 3 bits --*/
void set_car(ipointer this,ipointer that) {
   assert(cbox_p(this));*this=((ulong)that);
}
/*}}}  */

/*{{{  set cdr of cbox, modifying the lowermost 3 bits --*/
void set_cdr(ipointer this,ipointer that) {
   assert(cbox_p(this));*(this+1)=((ulong)that);
}
/*}}}  */

/*{{{  set car of cbox, not modifying the lowermost 3 bits --*/
static void set_car_nomodify(ipointer this,ipointer that) {
   assert(cbox_p(this));
   *this=(((ulong)that & ~0x07L) | (*this & 0x07L));
}
/*}}}  */

/*{{{  set cdr of cbox, not modifying the lowermost 3 bits --*/
static void set_cdr_nomodify(ipointer this,ipointer that) {
   assert(cbox_p(this));
   *(this+1)=((ulong)that & ~0x07L) | (*(this+1) & 0x07L);
}
/*}}}  */

/*{{{  set the pointer to the next free block of a free storage element --*/
static void set_freeptr(ipointer this,ipointer that) {
   assert(storage_p(this));
   *(this+1)=(ulong)that;
}
/*}}}  */

/*{{{  get the pointer to the next free block of a free storage element --*/
static ipointer get_freeptr(ipointer this) {
   assert(storage_p(this));
   return (ipointer)(*(this+1));
}
/*}}}  */

/*{{{  set the size of a storage element --*/
static void set_size(ipointer cur,ulong size) {
   assert(even_p(size) && size<=0x10000L);
   assert(storage_p(cur));
   if (size==0x10000L) size=0;
   *cur=(*cur & 0xFFFFL) | (size<<16);
}
/*}}}  */

/*{{{  get the size of a storage element --*/
static ulong get_size(ipointer cur) {
   ulong w;
   assert(storage_p(cur));
   w=(*cur>>16 & 0xFFFFL);
   if (w==0) w=0x10000L;
   assert(even_p(w));
   return w;
}
/*}}}  */

/*{{{  set the typedescriptor of a storage element --*/
void set_typedesc(ipointer cur,uint td) {
   assert(storage_p(cur));
   *cur=(*cur & ~0xFFFEL) | (((ulong)td & 0x7FFFL)<<1);
}
/*}}}  */

/*{{{  get the typedescriptor of a storage element --*/
uint get_typedesc(ipointer cur) {
   assert(storage_p(cur));
   return (uint)((*cur>>1) & 0x7FFFL);
}
/*}}}  */

/* ======================================================================== */
/* Garbage collector routines                                               */
/* ======================================================================== */

/*{{{  garbage collector --*/
void garbage_collect(void) {
   ipointer pointer;
   printf("Garbage collector running...");
   #ifdef DEBUGMEM
   printf("\n");
   statistics_mem();
   #endif
   /* Call the mark algorithm for the stack elements */
   pointer=stack_ptr;
   while ((ulong)pointer<(ulong)revstack_ptr) {
      if (!special_p((ipointer )(*pointer)) && (ipointer)*pointer!=NIL) {
         #ifdef DEBUGMEM
         printf("GC: marking from stack-pointer 0x%lX\n",*pointer);
         #endif
         mark((ipointer )(*pointer));
      }
      pointer++;
   }
   /* Call the mark algorithm for the machine registers */
   if (!special_p(val_reg) && val_reg!=NIL)  {
      #ifdef DEBUGMEM
      printf("GC: marking from val register  0x%lX\n",(ulong)val_reg);
      #endif
      mark(val_reg);
   }
   if (!special_p(env_reg) && env_reg!=NIL) {
      #ifdef DEBUGMEM
      printf("GC: marking from env register  0x%lX\n",(ulong)env_reg);
      #endif
      mark(env_reg);
   }
   if (!special_p(fun_reg) && fun_reg!=NIL) {
      #ifdef DEBUGMEM
      printf("GC: marking from fun register  0x%lX\n",(ulong)fun_reg);
      #endif
      mark(fun_reg);
   }
   if (!special_p(argl_reg) && argl_reg!=NIL) {
      #ifdef DEBUGMEM
      printf("GC: marking from argl register 0x%lX\n",(ulong)argl_reg);
      #endif
      mark(argl_reg);
   }
   if (!special_p(exp_reg) && exp_reg!=NIL) {
      #ifdef DEBUGMEM
      printf("GC: marking from exp register  0x%lX\n",(ulong)exp_reg);
      #endif
      mark(exp_reg);
   }
   if (!special_p(unev_reg) && unev_reg!=NIL) {
      #ifdef DEBUGMEM
      printf("GC: marking from unev register 0x%lX\n",(ulong)unev_reg);
      #endif
      mark(unev_reg);
   }
   /* Now sweep */
   sweep_cbox();
   sweep_storage();
   #ifdef DEBUGMEM
   statistics_mem();
   #endif
   printf("done.\n");
}
/*}}}  */

/*{{{  sweep phase for consbox area --*/
static void sweep_cbox(void) {
   ipointer pointer;
   cbox_free=NIL;
   pointer=cbslbase;
   while ((ulong)pointer<(ulong)(cbslbase+CBSLD)) {
      if (car_unmarked_p(pointer)) {
         assert(cdr_unmarked_p(pointer));
         set_car(pointer,NIL);
         set_cdr(pointer,cbox_free);
         cbox_free=pointer;
      }
      else {
         assert(!cdr_unmarked_p(pointer));
         unset_car_mark(pointer);
         unset_cdr_mark(pointer);
      }
      pointer=pointer+2;
   }
}
/*}}}  */

/*{{{  sweep phase fo storage area --*/
static void sweep_storage(void) {
   ipointer pointer;
   ulong size;
   stor_free=NIL;
   pointer=dslbase;
   while ((ulong)pointer<(ulong)(dslbase+DSLD)) {
      size=0;
      if (!storage_unmarked_p(pointer)) {
         unset_storage_mark(pointer);
         pointer=pointer+get_size(pointer);
      }
      else {
         while ((ulong)(pointer+size)<(ulong)(dslbase+DSLD) &&
                storage_unmarked_p(pointer+size) &&
                size<=65536L) {
            size=size+get_size(pointer+size);
         }
         assert(even_p(size) && size>1);
         if (size>65536L) {
            set_size(pointer,65536L);
            size=size-65536L;
            set_freeptr(pointer,stor_free);
            stor_free=pointer;
            pointer=pointer+65536L;
            set_size(pointer,size);
         }
         else {
            set_size(pointer,size);
            set_freeptr(pointer,stor_free);
            stor_free=pointer;
            pointer=pointer+size;
         }
      }
   }
}
/*}}}  */

/*{{{  nonrecursive mark algorithm --*/
static void mark(ipointer cur) {
   ipointer prev,tmp,next;
   bool     stop=FALSE;
   assert(!special_p(cur) && cur!=NIL);
   prev=NIL;
   /* We try to set the mark of the element pointed to by cur */
   do {
      assert(cbox_p(cur) || storage_p(cur));
      if (storage_p(cur)) {
         set_storage_mark(cur);stop=(prev==NIL);
      }
      else if (car_unmarked_p(cur)) {
         assert(cdr_unmarked_p(cur));
         /* Advance over car */
         set_car_mark(cur);
         if (!special_p(car(cur)) && car(cur)!=NIL) {
            next=car(cur);
            if (cbox_p(next)) {
               if (car_unmarked_p(next)) {
                  assert(cdr_unmarked_p(next));
                  tmp=cur;
                  cur=next;
                  set_car_nomodify(tmp,prev);
                  prev=tmp;
               }
            }
            else {
               assert(storage_p(next));
               set_storage_mark(next);
            }
         }
      }
      else if (cdr_unmarked_p(cur)) {
         /* Advance over cdr */
         set_cdr_mark(cur);
         if (!special_p(cdr(cur)) && cdr(cur)!=NIL) {
            next=cdr(cur);
            if (cbox_p(next)) {
               if (car_unmarked_p(next)) {
                  assert(cdr_unmarked_p(next));
                  tmp=cur;
                  cur=next;
                  set_cdr_nomodify(tmp,prev);
                  prev=tmp;
               }
            }
            else {
               assert(storage_p(next));
               set_storage_mark(next);
            }
         }
      }
      else if (prev==NIL) {
         stop=TRUE;
      }
      else if (cdr_unmarked_p(prev)) {
         assert(!car_unmarked_p(prev));
         /* Retreat over car */
         tmp=prev;
         prev=car(prev);
         set_car_nomodify(tmp,cur);
         cur=tmp;
      }
      else {
         assert(!car_unmarked_p(prev));
         /* Retreat over cdr */
         tmp=prev;
         prev=cdr(prev);
         set_cdr_nomodify(tmp,cur);
         cur=tmp;
      }
   } while (!stop);
}
/*}}}  */

/*{{{  Check-up on memory --*/
void dump_state(void) {
   ipointer pc,ps;
   ulong size;
   bool pcstate,psstate;
   printf("Consboxes                               Storage\n");
   printf("---------                               -------\n");
   pc=cbslbase;
   ps=dslbase;
   pcstate=TRUE;
   psstate=TRUE;
   while ((ulong)pc<(ulong)(cbslbase+CBSLD) || (ulong)ps<(ulong)(dslbase+DSLD)) {
      if ((ulong)pc<(ulong)(cbslbase+CBSLD)) {
         if (pcstate==TRUE) {
            printf("%8lX: [%8lX] ",(ulong)pc,(*pc & ~0x01L));
            if ((*pc & 0x01L)!=0) printf("*"); else printf(" ");
            pcstate=FALSE;pc++;
         }
         else {
            printf("          [%8lX] ",(*pc & ~0x01L));
            if ((*pc & 0x01L)!=0) printf("*"); else printf(" ");
            pcstate=TRUE;pc++;
         }
         printf("                   ");
      }
      else {
         printf("                                      ");
      }
      if ((ulong)ps<(ulong)(dslbase+DSLD)) {
         if (psstate==TRUE) {
            printf("%8lX: [%8lX]",(ulong)ps,(*ps & ~0x01L));
            if ((*ps & 0x01L)!=0) printf("*"); else printf(" ");
            printf("(%lu %u)",get_size(ps),get_typedesc(ps));
            psstate=FALSE;size=get_size(ps)-1;ps++;
         }
         else {
            printf("          [%8lX] (%c%c%c%c)",(*ps),
            printit(*((char *)ps)),printit(*((char *)ps+1)),
            printit(*((char *)ps+2)),printit(*((char *)ps+3)));
            ps++;size--;
            if (size==0) psstate=TRUE;
         }
      }
      printf("\n");
   }
}
/*}}}  */

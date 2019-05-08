/* ===========================================================================
   Special values, typedescriptors and some magic
   ----------------------------------------------
   Magic.c assumes that a string, as expected by "printf()" is stored
   in a continguous storage area. (i.e. the characters are packed).

   Typedescriptor structure
   ------------------------
   Data stored in the interpreter's memory has got a 15-bit typedescriptor
   to describe what has been stored. This typedescriptor is of type
   "unsigned int".

   Strings:  "type" = STRING_STORAGE;
             the upper 7 bits are useless: the string is '\0'-terminated.
   Symbols:  "type" = SYMBOL_STORAGE;
             the upper 7 bits are useless: the string is '\0'-terminated.
   Integers: "type" = INTEGER_STORAGE;
             a signed longint has been stored; Values that are up to 16
             bits long are stored as special values (see below).

                                 32
                                 |
                        +--------+---------+
                        |                  |
                       16                 16
                 (size of block)           |
                                  +--------+--------+
                                  |                 |
                                 15                 1
                          (typedescriptor)       (mark bit)

   Special values for ZAP_SPECIAL
   ------------------------------
   Pointers that have their special bits set to "ZAP_SPECIAL" don't point
   to data, but contain data themselves. A special pointer is structured
   like this:
                           32
                           |
                   +-------+-------------+
                   |                     |
                  16                    16
                   |                     |
           +-------+------+       +------+-----------+
           |              |       |                  |
           8              8       8                  8
       (data B1)       (data B0) (data A)            |
                                             +-------+---------+
                                             |                 |
                                             5                 3
                                           (type)              |
                                                         +-----+------+
                                                         2            1
                                                  (special bits)  (mark bit)

   The following types are supported as zap_special values:

   Booleans      : (type BOOL_MAGIC). These are always special values.
                   DataA is 0 if FALSE, 1 if TRUE.

   Characters    : (type CHAR_MAGIC). These are always special values.
                   16 Bit in character in DataA, considered signed.

   Short Strings : Up to 3 characters may be stored.
                   Type==STRING_MAGIC_0: Null string
                   Type==STRING_MAGIC_1: 1-char string (DataA)
                   Type==STRING_MAGIC_2: 2-char string (DataB0,A)
                   Type==STRING_MAGIC_3: 3-char string (DataB1,B0,A)

   Small Integers: Type is SHORT_MAGIC
                   Integers between -0x8000 and 0x7FFFF can be stored in
                   DataB.

   Short symbols : Symbols of up to 3 characters may be stored, in the same
                   way as strings. Types are:
                   Type==SYM_MAGIC_1 (DataA)
                   Type==SYM_MAGIC_2 (DataB0,A)
                   Type==SYM_MAGIC_3 (DataB1,B0,A)

   Keywords
   --------
   Some heavily used symbols have been predefined; their pointers are fixed.
   If this is a real pointer, each occurrence of the same symbol points
   to the same storage place; duplication of the symbol is avoided.
   If the symbol is a function identifier, the unique pointer is also used
   as the function key. Finally, these symbols are all "reserved"; you
   cannot define or set! them. The symbols in question (or their pointers)
   have been stored in a linked list, so that they may be found by the
   garbage collector.

   Environment structure
   ---------------------
   Several procedures in this section have to know about the environment
   structure.

   An environment is just a linked list. The car of each element points to
   pairs ("bindings") that give
   - The symbol in the car
   - The value  in the cdr

   The first consbox of the environment is an "environment header";
   the car of this cons-box points to the parent environment, the cdr to
   the rest of the environment structure.

        [+|+] (header)
           |
         [+|+]------[+|+]-----[+|+]------[+|+]------[+|X]
          |          |         |          |          |
        [+|+]      [+|+]     [+|+]      [+|+]      [+|+]
        /   \      /   \     /   \      /   \      /   \
      sym   val  sym   val sym   val  sym   val  sym   val

   The cdr of the header has its special bits set to ENV_SPECIAL.

   Procedures
   ----------
   Procedures are a pair (procedure-text.environment-pointer)
   in the case of compound procedures; or a pair (key.NULL) in the case
   of build-in procedures. The key is the zap-value of the reserved
   procedure symbol. It may happen that this is only a reserved word
   but that no procedure corresponds to this word as is the case with "else".
   Evaluating "else" will indeed give a built-in procedure, but applying
   this procedure will result in an error.

============================================================================ */

/*{{{  includes --*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define NDEBUG
#include <assert.h>
#include "memory.h"
#include "help.h"
#include "magic.h"
/*}}}  */

#define DEBUGMAGIC    /* Debugging on */
#undef  DEBUGMAGIC

/*{{{  definitions for zap types --*/
static const uint BOOL_MAGIC     = 0;
static const uint CHAR_MAGIC     = 1;
static const uint STRING_MAGIC_0 = 2;
static const uint STRING_MAGIC_1 = 3;
static const uint STRING_MAGIC_2 = 4;
static const uint STRING_MAGIC_3 = 5;
static const uint SHORT_MAGIC    = 7;
static const uint SYM_MAGIC_1    = 8;
static const uint SYM_MAGIC_2    = 9;
static const uint SYM_MAGIC_3    = 10;
/*}}}  */

/*{{{  other definitions --*/
static const int WRITENODES = 200;  /* No. nodes that write() will print */
static ipointer keyword_pointer;    /* Pointer to list of const pointers */
/*}}}  */

/*{{{  storage type definitions --*/
static const uint STRING_STORAGE  = 0;
static const uint INTEGER_STORAGE = 1;
static const uint SYMBOL_STORAGE  = 2;
/*}}}  */

/*{{{  definition of constants (zap values & pointers to keyword symbols) --*/
ipointer true_zap;
ipointer false_zap;
ipointer mult_zap;
ipointer add_zap;
ipointer sub_zap;
ipointer div_zap;
ipointer small_zap;
ipointer smalleq_zap;
ipointer eqarith_zap;
ipointer bigger_zap;
ipointer bigeq_zap;
ipointer and_zap;
ipointer or_zap;
ipointer not_zap;
ipointer car_zap;
ipointer cdr_zap;
ipointer cadr_zap;
ipointer cdar_zap;
ipointer cddr_zap;
ipointer caar_zap;
ipointer caaar_zap;
ipointer caadr_zap;
ipointer cadar_zap;
ipointer caddr_zap;
ipointer cdaar_zap;
ipointer cdadr_zap;
ipointer cddar_zap;
ipointer cdddr_zap;
ipointer caaaar_zap;
ipointer caaadr_zap;
ipointer caadar_zap;
ipointer caaddr_zap;
ipointer cadaar_zap;
ipointer cadadr_zap;
ipointer caddar_zap;
ipointer cadddr_zap;
ipointer cdaaar_zap;
ipointer cdaadr_zap;
ipointer cdadar_zap;
ipointer cdaddr_zap;
ipointer cddaar_zap;
ipointer cddadr_zap;
ipointer cdddar_zap;
ipointer cddddr_zap;
ipointer let_zap;
ipointer gcstat_zap;
ipointer quote_zap;
ipointer cond_zap;
ipointer if_zap;
ipointer else_zap;
ipointer cons_zap;
ipointer define_zap;
ipointer error_zap;
ipointer integerp_zap;
ipointer lambda_zap;
ipointer length_zap;
ipointer list_zap;
ipointer newline_zap;
ipointer nullp_zap;
ipointer numberp_zap;
ipointer oddp_zap;
ipointer pairp_zap;
ipointer eqp_zap;
ipointer stringp_zap;
ipointer symbolp_zap;
ipointer evenp_zap;
ipointer listp_zap;
ipointer setw_zap;
ipointer setcarw_zap;
ipointer setcdrw_zap;
ipointer read_zap;
ipointer write_zap;
ipointer memdump_zap;
ipointer garbagecollect_zap;
ipointer synchecktoggle_zap;
ipointer gcstatwrite_zap;
/*}}}  */

/*{{{  procedure headers --*/
/*{{{  unparser*/
static void      write_recursive(ipointer cur,int *ndp);
static void      write_list(ipointer list,int *ndp);
/*}}}  */

/*{{{  setting storage data --*/
static void      set_data_storage_string(ipointer cur,char *val);
static void      set_data_storage_symbol(ipointer cur,char *val);
static void      set_data_storage_integer(ipointer cur,long int val);
/*}}}  */

/*{{{  extracting storage data --*/
static char     *extract_data_storage_string(ipointer cur);
static char     *extract_data_storage_symbol(ipointer cur);
static long int  extract_data_storage_integer(ipointer cur);

static char     *extract_zap_string(ipointer cur,int len);
/*}}}  */

/*{{{  reading and writing zap type --*/
static uint      get_zap_type(ipointer cur);
static ipointer  set_zap_type(ipointer cur,uint type);
/*}}}  */

/*{{{  reading and writing zap data --*/
static uchar     get_zap_dataA(ipointer cur);
static uchar     get_zap_dataB0(ipointer cur);
static uchar     get_zap_dataB1(ipointer cur);
static uint      get_zap_dataB(ipointer cur);
static ipointer  set_zap_dataA(ipointer cur,uchar ch);
static ipointer  set_zap_dataB0(ipointer cur,uchar ch);
static ipointer  set_zap_dataB1(ipointer cur,uchar ch);
static ipointer  set_zap_dataB(ipointer cur,uint val);
/*}}}  */
/*}}}  */

/* ========================================================================= */
/* Management of constant pointers                                           */
/* ========================================================================= */

/*{{{  initialization of constant pointers --*/
/* The created structures are inserted into a list; a pointer to this list   */
/* is put on the stack so that the garbage collector may find it.            */
/* Care has to be taken that the chain is all right before calling new_cons  */
/* Moreover, keyword_pointer must be initialized only at the end, because    */
/* make-symbol traverses the keyword chain before creating a symbol.         */

void init_magic(void) {
   ipointer p,psave;
   /* The values for true & false... */
   true_zap     = make_bool(TRUE);
   false_zap    = make_bool(FALSE);
   /* A bunch of symbols are put into a list */
   /* these are "reserved symbols", and denote built-in procedures */
   p=new_cons();revpush_pointer(p);psave=p;keyword_pointer=NIL;
   mult_zap     = make_symbol("*");
   set_car(p,mult_zap);set_cdr(p,new_cons());p=cdr(p);
   add_zap      = make_symbol("+");
   set_car(p,add_zap);set_cdr(p,new_cons());p=cdr(p);
   sub_zap      = make_symbol("-");
   set_car(p,sub_zap);set_cdr(p,new_cons());p=cdr(p);
   div_zap      = make_symbol("/");
   set_car(p,div_zap);set_cdr(p,new_cons());p=cdr(p);
   small_zap    = make_symbol("<");
   set_car(p,small_zap);set_cdr(p,new_cons());p=cdr(p);
   smalleq_zap  = make_symbol("<=");
   set_car(p,smalleq_zap);set_cdr(p,new_cons());p=cdr(p);
   eqarith_zap  = make_symbol("=");
   set_car(p,eqarith_zap);set_cdr(p,new_cons());p=cdr(p);
   bigger_zap   = make_symbol(">");
   set_car(p,bigger_zap);set_cdr(p,new_cons());p=cdr(p);
   bigeq_zap    = make_symbol(">=");
   set_car(p,bigeq_zap);set_cdr(p,new_cons());p=cdr(p);
   and_zap      = make_symbol("and");
   set_car(p,and_zap);set_cdr(p,new_cons());p=cdr(p);
   or_zap       = make_symbol("or");
   set_car(p,or_zap);set_cdr(p,new_cons());p=cdr(p);
   not_zap      = make_symbol("not");
   set_car(p,not_zap);set_cdr(p,new_cons());p=cdr(p);
   car_zap      = make_symbol("car");
   set_car(p,car_zap);set_cdr(p,new_cons());p=cdr(p);
   cdr_zap      = make_symbol("cdr");
   set_car(p,cdr_zap);set_cdr(p,new_cons());p=cdr(p);
   cadr_zap     = make_symbol("cadr");
   set_car(p,cadr_zap);set_cdr(p,new_cons());p=cdr(p);
   cdar_zap     = make_symbol("cdar");
   set_car(p,cdar_zap);set_cdr(p,new_cons());p=cdr(p);
   cddr_zap     = make_symbol("cddr");
   set_car(p,cddr_zap);set_cdr(p,new_cons());p=cdr(p);
   caar_zap     = make_symbol("caar");
   set_car(p,caar_zap);set_cdr(p,new_cons());p=cdr(p);
   cond_zap     = make_symbol("cond");
   set_car(p,cond_zap);set_cdr(p,new_cons());p=cdr(p);
   if_zap       = make_symbol("if");
   set_car(p,if_zap);set_cdr(p,new_cons());p=cdr(p);
   else_zap     = make_symbol("else");
   set_car(p,else_zap);set_cdr(p,new_cons());p=cdr(p);
   cons_zap     = make_symbol("cons");
   set_car(p,cons_zap);set_cdr(p,new_cons());p=cdr(p);
   define_zap   = make_symbol("define");
   set_car(p,define_zap);set_cdr(p,new_cons());p=cdr(p);
   error_zap    = make_symbol("error");
   set_car(p,error_zap);set_cdr(p,new_cons());p=cdr(p);
   integerp_zap = make_symbol("integer?");
   set_car(p,integerp_zap);set_cdr(p,new_cons());p=cdr(p);
   lambda_zap   = make_symbol("lambda");
   set_car(p,lambda_zap);set_cdr(p,new_cons());p=cdr(p);
   length_zap   = make_symbol("length");
   set_car(p,length_zap);set_cdr(p,new_cons());p=cdr(p);
   list_zap     = make_symbol("list");
   set_car(p,list_zap);set_cdr(p,new_cons());p=cdr(p);
   newline_zap  = make_symbol("newline");
   set_car(p,newline_zap);set_cdr(p,new_cons());p=cdr(p);
   nullp_zap    = make_symbol("null?");
   set_car(p,nullp_zap);set_cdr(p,new_cons());p=cdr(p);
   numberp_zap  = make_symbol("number?");
   set_car(p,numberp_zap);set_cdr(p,new_cons());p=cdr(p);
   oddp_zap     = make_symbol("odd?");
   set_car(p,oddp_zap);set_cdr(p,new_cons());p=cdr(p);
   pairp_zap    = make_symbol("pair?");
   set_car(p,pairp_zap);set_cdr(p,new_cons());p=cdr(p);
   eqp_zap      = make_symbol("eq?");
   set_car(p,eqp_zap);set_cdr(p,new_cons());p=cdr(p);
   let_zap      = make_symbol("let");
   set_car(p,let_zap);set_cdr(p,new_cons());p=cdr(p);
   stringp_zap  = make_symbol("string?");
   set_car(p,stringp_zap);set_cdr(p,new_cons());p=cdr(p);
   symbolp_zap  = make_symbol("symbol?");
   set_car(p,symbolp_zap);set_cdr(p,new_cons());p=cdr(p);
   evenp_zap    = make_symbol("even?");
   set_car(p,evenp_zap);set_cdr(p,new_cons());p=cdr(p);
   listp_zap    = make_symbol("list?");
   set_car(p,listp_zap);set_cdr(p,new_cons());p=cdr(p);
   setw_zap     = make_symbol("set!");
   set_car(p,setw_zap);set_cdr(p,new_cons());p=cdr(p);
   setcarw_zap  = make_symbol("set-car!");
   set_car(p,setcarw_zap);set_cdr(p,new_cons());p=cdr(p);
   setcdrw_zap  = make_symbol("set-cdr!");
   set_car(p,setcdrw_zap);set_cdr(p,new_cons());p=cdr(p);
   read_zap     = make_symbol("read");
   set_car(p,read_zap);set_cdr(p,new_cons());p=cdr(p);
   write_zap    = make_symbol("write");
   set_car(p,write_zap);set_cdr(p,new_cons());p=cdr(p);
   quote_zap    = make_symbol("quote");
   set_car(p,quote_zap);set_cdr(p,new_cons());p=cdr(p);
   caaar_zap    = make_symbol("caaar");
   set_car(p,caaar_zap);set_cdr(p,new_cons());p=cdr(p);
   caadr_zap    = make_symbol("caadr");
   set_car(p,caadr_zap);set_cdr(p,new_cons());p=cdr(p);
   cadar_zap    = make_symbol("cadar");
   set_car(p,cadar_zap);set_cdr(p,new_cons());p=cdr(p);
   caddr_zap    = make_symbol("caddr");
   set_car(p,caddr_zap);set_cdr(p,new_cons());p=cdr(p);
   cdaar_zap    = make_symbol("cdaar");
   set_car(p,cdaar_zap);set_cdr(p,new_cons());p=cdr(p);
   cdadr_zap    = make_symbol("cdadr");
   set_car(p,cdadr_zap);set_cdr(p,new_cons());p=cdr(p);
   cddar_zap    = make_symbol("cddar");
   set_car(p,cddar_zap);set_cdr(p,new_cons());p=cdr(p);
   cdddr_zap    = make_symbol("cdddr");
   set_car(p,cdddr_zap);set_cdr(p,new_cons());p=cdr(p);
   caaaar_zap   = make_symbol("caaaar");
   set_car(p,caaaar_zap);set_cdr(p,new_cons());p=cdr(p);
   caaadr_zap   = make_symbol("caaadr");
   set_car(p,caaadr_zap);set_cdr(p,new_cons());p=cdr(p);
   caadar_zap   = make_symbol("caadar");
   set_car(p,caadar_zap);set_cdr(p,new_cons());p=cdr(p);
   caaddr_zap   = make_symbol("caaddr");
   set_car(p,caaddr_zap);set_cdr(p,new_cons());p=cdr(p);
   cadaar_zap   = make_symbol("cadaar");
   set_car(p,cadaar_zap);set_cdr(p,new_cons());p=cdr(p);
   cadadr_zap   = make_symbol("cadadr");
   set_car(p,cadadr_zap);set_cdr(p,new_cons());p=cdr(p);
   caddar_zap   = make_symbol("caddar");
   set_car(p,caddar_zap);set_cdr(p,new_cons());p=cdr(p);
   cadddr_zap   = make_symbol("cadddr");
   set_car(p,cadddr_zap);set_cdr(p,new_cons());p=cdr(p);
   cdaaar_zap   = make_symbol("cdaaar");
   set_car(p,cdaaar_zap);set_cdr(p,new_cons());p=cdr(p);
   cdaadr_zap   = make_symbol("cdaadr");
   set_car(p,cdaadr_zap);set_cdr(p,new_cons());p=cdr(p);
   cdadar_zap   = make_symbol("cdadar");
   set_car(p,cdadar_zap);set_cdr(p,new_cons());p=cdr(p);
   cdaddr_zap   = make_symbol("cdaddr");
   set_car(p,cdaddr_zap);set_cdr(p,new_cons());p=cdr(p);
   cddaar_zap   = make_symbol("cddaar");
   set_car(p,cddaar_zap);set_cdr(p,new_cons());p=cdr(p);
   cddadr_zap   = make_symbol("cddadr");
   set_car(p,cddadr_zap);set_cdr(p,new_cons());p=cdr(p);
   cdddar_zap   = make_symbol("cdddar");
   set_car(p,cdddar_zap);set_cdr(p,new_cons());p=cdr(p);
   cddddr_zap   = make_symbol("cddddr");
   set_car(p,cddddr_zap);set_cdr(p,new_cons());p=cdr(p);
   gcstat_zap   = make_symbol("gcstat");
   set_car(p,gcstat_zap);set_cdr(p,new_cons());p=cdr(p);
   memdump_zap  = make_symbol("memdump");
   set_car(p,memdump_zap);set_cdr(p,new_cons());p=cdr(p);
   garbagecollect_zap = make_symbol("garbagecollect");
   set_car(p,garbagecollect_zap);set_cdr(p,new_cons());p=cdr(p);
   synchecktoggle_zap = make_symbol("synchecktoggle");
   set_car(p,synchecktoggle_zap);set_cdr(p,new_cons());p=cdr(p);
   gcstatwrite_zap = make_symbol("gcstatwrite");
   set_car(p,gcstatwrite_zap);
   keyword_pointer=psave;
}
/*}}}  */

/*{{{  check whether a symbol is a reserved word --*/
bool reserved_p(ipointer cur) {
   ipointer p;
   assert(symbol_p(cur));
   p=keyword_pointer;
   while (p!=NIL && !equal_p(cur,car(p))) p=cdr(p);
   return (p!=NIL);
}
/*}}}  */

/* ========================================================================= */
/* write()-procedure: Dumps a structure to stdout. This is recursive!        */
/* ========================================================================= */

/*{{{  initially called function --*/
void write_call(ipointer cur) {
   int nodesprinted=0;
   write_recursive(cur,&nodesprinted);
   printf("\n");
}
/*}}}  */

/*{{{  printout of an arbitrary element --*/
static void write_recursive(ipointer cur,int *ndp) {
   if (*ndp<WRITENODES) {
      *ndp=*ndp+1;
      if (cur==NIL) {
         printf("()");
      }
      else if (bool_p(cur)) {
         if (bool_of(cur)) printf("#T");
         else printf("#F");
      }
      else if (char_p(cur)) {
         if (char_of(cur)<0 || char_p(cur)>255) printf("#\\-");
         else printf("#\\%c",printit((char)char_of(cur)));
      }
      else if (string_p(cur)) {
         printf("\"%s\"",string_of(cur));
      }
      else if (integer_p(cur)) {
         printf("%li",integer_of(cur));
      }
      else if (symbol_p(cur)) {
         printf("%s",symbol_of(cur));
      }
      else if (cbox_p(cur) && hint_environment_p(cur)) {
         printf("[ -- Environment -- Parent: 0x%X -- ]\n",(ulong)parent(cur));
         cur=first_frame(cur);
         while (cur!=NIL && *ndp<WRITENODES) {
            printf("[");
            write_recursive(first_binding(cur),ndp);
            printf("]\n");
            cur=rest_bindings(cur);
         }
      }
      else if (cbox_p(cur) && hint_procedure_p(cur)) {
         if (proc_env(cur)==NIL) {
            printf("[Reserved word :: 0x%lX]",(ulong)proc_text(cur));
         }
         else {
            printf("[Compound-procedure :: 0x%lX | 0x%lX]",
                     (ulong)proc_text(cur),(ulong)proc_env(cur));
         }
      }
      else if (cbox_p(cur)) {
         printf("(");
         write_list(cur,ndp);
         printf(")");
      }
      else {
         printf("PROGRAM ERROR: write_recursive(): unknown type.\n");
      }
   }
}
/*}}}  */

/*{{{  printout of a list --*/
static void write_list(ipointer list,int *ndp) {
   if (*ndp<WRITENODES) {
      *ndp=*ndp+1;
      if (cbox_p(cdr(list))) {
         write_recursive(car(list),ndp);
         printf(" ");
         write_list(cdr(list),ndp);
      }
      else if (cdr(list)==NIL) {
         write_recursive(car(list),ndp);
      }
      else {
         write_recursive(car(list),ndp);
         printf(" . ");
         write_recursive(cdr(list),ndp);
      }
   }
}
/*}}}  */

/* ========================================================================= */
/* Creation of elements                                                      */
/* ========================================================================= */

/*{{{  creation of a boolean --*/
ipointer make_bool(bool val) {
   ipointer p;
   #ifdef DEBUGMAGIC
   if (val) printf("magic.c: make_bool() called with TRUE.\n");
   else     printf("magic.c: make_bool() called with FALSE.\n");
   #endif
   p=set_zap_type((ipointer)0L,BOOL_MAGIC);
   p=set_zap_dataA(p,(uchar)val);
   return set_zap_special(p);
}
/*}}}  */

/*{{{  creation of a symbol --*/
ipointer make_symbol(char *val) {
   uint     i;
   ipointer p;
   #ifdef DEBUGMAGIC
   printf("magic.c: make_symbol() called with \"%s\".\n",val);
   #endif
   for (i=0;val[i]!='\0';i++);
   if (i<=3) {
      assert(i!=0);
      if (i==1) {
         p=set_zap_type((ipointer)0,SYM_MAGIC_1);
         p=set_zap_dataA(p,(uchar)val[0]);
         p=set_zap_special(p);
      }
      else if (i==2) {
         p=set_zap_type((ipointer)0,SYM_MAGIC_2);
         p=set_zap_dataA(p,(uchar)val[1]);
         p=set_zap_dataB0(p,(uchar)val[0]);
         p=set_zap_special(p);
      }
      else if (i==3) {
         p=set_zap_type((ipointer)0,SYM_MAGIC_3);
         p=set_zap_dataA(p,(uchar)val[2]);
         p=set_zap_dataB0(p,(uchar)val[1]);
         p=set_zap_dataB1(p,(uchar)val[0]);
         p=set_zap_special(p);
      }
   }
   else {
      p=keyword_pointer;
      while (p!=NIL && strcmp(symbol_of(car(p)),val)!=0) p=cdr(p);
      if (p==NIL) {
         p=new_storage((ulong)((sizeof(char))*(i+1)));
         set_data_storage_symbol(p,val);
      }
      else {
         p=car(p);
      }
   }
   #ifdef DEBUGMAGIC
   if (special_p(p)) {
      printf("make_symbol(): compressing to 0x%lX.\n",(ulong)p);
   }
   #endif
   return p;
}
/*}}}  */

/*{{{  creation of a string --*/
ipointer make_string(char *val) {
   uint     i;
   ipointer p;
   #ifdef DEBUGMAGIC
   printf("make_string() called with \"%s\".\n",val);
   #endif
   for (i=0;val[i]!='\0';i++);
   if (i==0) {
      p=set_zap_type((ipointer)0,STRING_MAGIC_0);
      p=set_zap_special(p);
   }
   else if (i==1) {
      p=set_zap_type((ipointer)0,STRING_MAGIC_1);
      p=set_zap_dataA(p,(uchar)val[0]);
      p=set_zap_special(p);
   }
   else if (i==2) {
      p=set_zap_type((ipointer)0,STRING_MAGIC_2);
      p=set_zap_dataA(p,(uchar)val[1]);
      p=set_zap_dataB0(p,(uchar)val[0]);
      p=set_zap_special(p);
   }
   else if (i==3) {
      p=set_zap_type((ipointer)0,STRING_MAGIC_3);
      p=set_zap_dataA(p,(uchar)val[2]);
      p=set_zap_dataB0(p,(uchar)val[1]);
      p=set_zap_dataB1(p,(uchar)val[0]);
      p=set_zap_special(p);
   }
   else {
      p=new_storage((ulong)(sizeof(char)*(i+1)));
      set_data_storage_string(p,val);
   }
   #ifdef DEBUGMAGIC
   if (i<4) {
      printf("make_string(): compressing to 0x%lX.\n",(ulong)p);
   }
   #endif
   return p;
}
/*}}}  */

/*{{{  creation of an integer --*/
ipointer make_int(long int val) {
   ipointer p;
   #ifdef DEBUGMAGIC
   printf("make_int() called with %li.\n",val);
   #endif
   if (val<=0x7FFFL && val>=-0x8000L) {
      p=set_zap_type((ipointer)0,SHORT_MAGIC);
      p=set_zap_dataB(p,(uint)val);
      p=set_zap_special(p);
      #ifdef DEBUGMAGIC
      printf("make_int(): compressing to 0x%lX.\n",(ulong)p);
      #endif
   }
   else {
      p=new_storage(sizeof(long int));
      set_data_storage_integer(p,val);
   }
   return p;
}
/*}}}  */

/*{{{  creation of a character --*/
/* integer values going from 0 to 255 are considered "normal" chars */
ipointer make_char(int val) {
   ipointer p;
   assert((long)val<=0x7FFFL && (long)val>=-0x8000L);
   #ifdef DEBUGMAGIC
   if (val<0 || val>255)
      printf("make_char() called with non-8-bit value.\n");
   else
      printf("make_char() called with %c.\n",printit((char)val));
   #endif
   p=set_zap_type((ipointer)0,CHAR_MAGIC);
   p=set_zap_dataB(p,(uint)val);
   return set_zap_special(p);
}
/*}}}  */

/* ========================================================================= */
/* Setting and getting the information stored in a storage element           */
/* ========================================================================= */

/*{{{  writing a string --*/
static void set_data_storage_string(ipointer cur,char *val) {
   assert(!special_p(cur) && storage_p(cur));
   strcpy((char *)(cur+1),val);
   set_typedesc(cur,STRING_STORAGE);
}
/*}}}  */

/*{{{  writing a symbol --*/
static void set_data_storage_symbol(ipointer cur,char *val) {
   assert(!special_p(cur) && storage_p(cur));
   strcpy((char *)(cur+1),val);
   set_typedesc(cur,SYMBOL_STORAGE);
}
/*}}}  */

/*{{{  writing an integer --*/
static void set_data_storage_integer(ipointer cur,long int val) {
   assert(!special_p(cur) && storage_p(cur));
   *(long int *)(cur+1)=val;
   set_typedesc(cur,INTEGER_STORAGE);
}
/*}}}  */

/*{{{  getting a string --*/
static char *extract_data_storage_string(ipointer cur) {
   assert(!special_p(cur) && storage_p(cur));
   assert(get_typedesc(cur)==STRING_STORAGE);
   return (char *)(cur+1);
}
/*}}}  */

/*{{{  getting a symbol --*/
static char *extract_data_storage_symbol(ipointer cur) {
   assert(!special_p(cur) && storage_p(cur));
   assert(get_typedesc(cur)==SYMBOL_STORAGE);
   return (char *)(cur+1);
}
/*}}}  */

/*{{{  getting an integer --*/
static long int extract_data_storage_integer(ipointer cur) {
   assert(!special_p(cur) && storage_p(cur));
   assert(get_typedesc(cur)==INTEGER_STORAGE);
   return *(long int *)(cur+1);
}
/*}}}  */

/* ========================================================================= */
/* Setting and reading the zap data elements                                 */
/* ========================================================================= */

/*{{{  setting --*/
static uchar get_zap_dataA(ipointer cur) {
   return (uchar)(((ulong)cur>>8) & 0xFFL);
}

static uchar get_zap_dataB0(ipointer cur) {
   return (uchar)(((ulong)cur>>16) & 0xFFL);
}

static uchar get_zap_dataB1(ipointer cur) {
   return (uchar)(((ulong)cur>>24) & 0xFFL);
}

static uint get_zap_dataB(ipointer cur) {
   return (uint)(((ulong)cur>>16) & 0xFFFFL);
}
/*}}}  */

/*{{{  reading --*/
static ipointer set_zap_dataA(ipointer cur,uchar ch) {
   return (ipointer)(((ulong)cur & ~0xFF00L) | (((ulong)ch & 0xFFL) << 8));
}

static ipointer set_zap_dataB0(ipointer cur,uchar ch) {
   return (ipointer)(((ulong)cur & ~0xFF0000L) | (((ulong)ch & 0xFFL) << 16));
}

static ipointer set_zap_dataB1(ipointer cur,uchar ch) {
   return (ipointer)(((ulong)cur & ~0xFF000000L) | (((ulong)ch & 0xFFL) << 24));
}

static ipointer set_zap_dataB(ipointer cur,uint val) {
   return (ipointer)(((ulong)cur & ~0xFFFF0000L) | (((ulong)val & 0xFFFFL)<<16));
}
/*}}}  */

/* ========================================================================= */
/* Setting and reading the zap type                                          */
/* ========================================================================= */

/*{{{  setting --*/
static uint get_zap_type(ipointer cur) {
   return (uint)(((ulong)cur>>3) & 0x1FL);
}
/*}}}  */

/*{{{  reading --*/
static ipointer set_zap_type(ipointer cur,uint type) {
   return (ipointer)(((ulong)cur & ~0xF8L) | (((ulong)type & 0x1FL)<<3));
}
/*}}}  */

/* ========================================================================= */
/* Extracting values                                                         */
/* ========================================================================= */

/*{{{  help procedure to extract strings --*/
static char *extract_zap_string(ipointer cur,int len) {
   static char s[4];
   int i=0;
   assert(0<=len && len<4);
   switch (len) {
   case 3: s[i++]=(char)get_zap_dataB1(cur);
           /* FALL-THROUGH */
   case 2: s[i++]=(char)get_zap_dataB0(cur);
           /* FALL-THROUGH */
   case 1: s[i++]=(char)get_zap_dataA(cur);
           /* FALL-THROUGH */
   case 0: s[i]='\0';
           break;
   }
   return s;
}
/*}}}  */

/*{{{  integer --*/
long int integer_of(ipointer x) {
   long int i;
   assert(integer_p(x));
   if (special_p(x)) {
      i=(long int)get_zap_dataB(x);
      if ((i & 0x8000L)!=0) i=(i | ~0xFFFFL);
      return i;
   }
   else {
      return extract_data_storage_integer(x);
   }
}
/*}}}  */

/*{{{  boolean --*/
bool bool_of(ipointer x) {
   assert(bool_p(x));
   return (x==true_zap);
}
/*}}}  */

/*{{{  symbol --*/
char *symbol_of(ipointer x) {
   uint a;
   assert(symbol_p(x));
   if (special_p(x)) {
      a=get_zap_type(x);
      assert(a==SYM_MAGIC_1 || a==SYM_MAGIC_2 || a==SYM_MAGIC_3);
      if         (a==SYM_MAGIC_1)    return extract_zap_string(x,1);
      else if    (a==SYM_MAGIC_2)    return extract_zap_string(x,2);
      else    /* (a==SYM_MAGIC_3) */ return extract_zap_string(x,3);
   }
   else return extract_data_storage_symbol(x);
}
/*}}}  */

/*{{{  string --*/
char *string_of(ipointer x) {
   uint a;
   assert(string_p(x));
   if (special_p(x)) {
      a=get_zap_type(x);
      assert(a==STRING_MAGIC_1 || a==STRING_MAGIC_2 || a==STRING_MAGIC_3 ||
             a==STRING_MAGIC_0);
      if         (a==STRING_MAGIC_0)    return "";
      else if    (a==STRING_MAGIC_1)    return extract_zap_string(x,1);
      else if    (a==STRING_MAGIC_2)    return extract_zap_string(x,2);
      else    /* (a==STRING_MAGIC_3) */ return extract_zap_string(x,3);
   }
   else return extract_data_storage_string(x);
}
/*}}}  */

/*{{{  character --*/
int char_of(ipointer x) {
   int i;
   assert(char_p(x));
   i=(int)get_zap_dataB(x);
   if ((i & 0x8000L)!=0) i=(int)(i | ~0xFFFFL);
   return i;
}
/*}}}  */

/* ========================================================================= */
/* Querying type                                                             */
/* ========================================================================= */

/*{{{  number? --*/
bool number_p(ipointer x) {
   if (special_p(x)) {
      return (get_zap_type(x)==SHORT_MAGIC);
   }
   else if (storage_p(x)) {
      return (get_typedesc(x)==INTEGER_STORAGE);
   }
   else return FALSE;
}
/*}}}  */

/*{{{  integer? --*/
bool integer_p(ipointer x) {
   if (special_p(x)) {
      return (get_zap_type(x)==SHORT_MAGIC);
   }
   else if (storage_p(x)) {
      return (get_typedesc(x)==INTEGER_STORAGE);
   }
   else return FALSE;
}
/*}}}  */

/*{{{  string? --*/
bool string_p(ipointer x) {
   uint a;
   if (special_p(x)) {
      a=get_zap_type(x);
      return ((a==STRING_MAGIC_0) || (a==STRING_MAGIC_1) ||
              (a==STRING_MAGIC_2) || (a==STRING_MAGIC_3));
   }
   else if (storage_p(x)) {
      return (get_typedesc(x)==STRING_STORAGE);
   }
   else return FALSE;
}
/*}}}  */

/*{{{  symbol? --*/
bool symbol_p(ipointer x) {
   uint a;
   if (special_p(x)) {
      a=get_zap_type(x);
      return (a==SYM_MAGIC_1 || a==SYM_MAGIC_2 || a==SYM_MAGIC_3);
   }
   else if (storage_p(x)) {
      return (get_typedesc(x)==SYMBOL_STORAGE);
   }
   else return FALSE;
}
/*}}}  */

/*{{{  character? --*/
bool char_p(ipointer x) {
   if (special_p(x)) {
      return (get_zap_type(x)==CHAR_MAGIC);
   }
   else return FALSE;
}
/*}}}  */

/*{{{  boolean? --*/
bool bool_p(ipointer x) {
   return (x==true_zap || x==false_zap);
}
/*}}}  */

/* ========================================================================= */
/* Comparing two elements (eq?)                                              */
/* ========================================================================= */

/* The comparison gives TRUE if both elements have the same type and have    */
/* same value; or if they point to the same storage box or cons-box          */

/*{{{  comparison --*/
bool equal_p(ipointer a,ipointer b) {
   uint i,j;
   if ((ulong)a==(ulong)b) {
      return TRUE;
   }
   else if (storage_p(a) && storage_p(b)) {
      i=get_typedesc(a);j=get_typedesc(b);
      if (i!=j) {
         return FALSE;
      }
      else {
         if (i==INTEGER_STORAGE) {
            return (extract_data_storage_integer(a)==
                    extract_data_storage_integer(b));
         }
         else if (i==STRING_STORAGE) {
            return (strcmp(extract_data_storage_string(a),
                           extract_data_storage_string(b))==0);
         }
         else if (i==SYMBOL_STORAGE) {
            return (strcmp(extract_data_storage_symbol(a),
                           extract_data_storage_symbol(b))==0);
         }
         else return FALSE;
      }
   }
   else return FALSE;
}
/*}}}  */

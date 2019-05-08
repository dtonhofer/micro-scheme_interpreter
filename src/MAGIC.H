#ifndef MAGIC_H
#define MAGIC_H

#include "memory.h"

/* Constant zap values; their value will be computed at startup time. */
/* They stand for heavily used symbols (booleans are included) */

extern ipointer true_zap;
extern ipointer false_zap;

/* ...of arithmetic operators */

extern ipointer mult_zap;
extern ipointer add_zap;
extern ipointer sub_zap;
extern ipointer div_zap;
extern ipointer small_zap;
extern ipointer smalleq_zap;
extern ipointer eqarith_zap;
extern ipointer bigger_zap;
extern ipointer bigeq_zap;

/* ...of logical operators */

extern ipointer and_zap;
extern ipointer or_zap;
extern ipointer not_zap;

/* ...of c****r */

extern ipointer car_zap;
extern ipointer cdr_zap;
extern ipointer cadr_zap;
extern ipointer cdar_zap;
extern ipointer cddr_zap;
extern ipointer caar_zap;
extern ipointer caaar_zap;
extern ipointer caadr_zap;
extern ipointer cadar_zap;
extern ipointer caddr_zap;
extern ipointer cdaar_zap;
extern ipointer cdadr_zap;
extern ipointer cddar_zap;
extern ipointer cdddr_zap;
extern ipointer caaaar_zap;
extern ipointer caaadr_zap;
extern ipointer caadar_zap;
extern ipointer caaddr_zap;
extern ipointer cadaar_zap;
extern ipointer cadadr_zap;
extern ipointer caddar_zap;
extern ipointer cadddr_zap;
extern ipointer cdaaar_zap;
extern ipointer cdaadr_zap;
extern ipointer cdadar_zap;
extern ipointer cdaddr_zap;
extern ipointer cddaar_zap;
extern ipointer cddadr_zap;
extern ipointer cdddar_zap;
extern ipointer cddddr_zap;

/* ...others */

extern ipointer let_zap;
extern ipointer gcstat_zap;
extern ipointer quote_zap;
extern ipointer cond_zap;
extern ipointer if_zap;
extern ipointer else_zap;
extern ipointer cons_zap;
extern ipointer define_zap;
extern ipointer error_zap;
extern ipointer integerp_zap;
extern ipointer lambda_zap;
extern ipointer length_zap;
extern ipointer list_zap;
extern ipointer newline_zap;
extern ipointer nullp_zap;
extern ipointer numberp_zap;
extern ipointer oddp_zap;
extern ipointer pairp_zap;
extern ipointer eqp_zap;
extern ipointer stringp_zap;
extern ipointer symbolp_zap;
extern ipointer evenp_zap;
extern ipointer listp_zap;
extern ipointer setw_zap;
extern ipointer setcarw_zap;
extern ipointer setcdrw_zap;
extern ipointer read_zap;
extern ipointer write_zap;
extern ipointer memdump_zap;
extern ipointer garbagecollect_zap;
extern ipointer synchecktoggle_zap;
extern ipointer gcstatwrite_zap;

/* Exported procedures */

extern bool      reserved_p(ipointer cur);
extern bool      equal_p(ipointer a,ipointer b);

extern void      write_call(ipointer cur);

extern ipointer  make_bool(bool val);
extern ipointer  make_symbol(char *val);
extern ipointer  make_string(char *val);
extern ipointer  make_int(long int val);
extern ipointer  make_char(int val);

extern void      init_magic(void);

extern long int  integer_of(ipointer x);
extern bool      bool_of(ipointer x);
extern char     *symbol_of(ipointer x);
extern int       char_of(ipointer x);
extern char     *string_of(ipointer x);

extern bool      symbol_p(ipointer x);
extern bool      char_p(ipointer x);
extern bool      bool_p(ipointer x);
extern bool      string_p(ipointer x);
extern bool      integer_p(ipointer x);
extern bool      number_p(ipointer x);

#endif

/* ===========================================================================
   Built-in procedures
   -------------------
   Here are "low-level" procedures that can (or have to) be implemented

=========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include "parser.h"
#include "memory.h"
#include "magic.h"
#include "main.h"
#include "help.h"
#include "builtin.h"
#include "math.h"

static ipointer apply_builtin1(ipointer proc,ipointer args);
static ipointer apply_builtin2(ipointer proc,ipointer args);

/* ======================================================================== */
/* Dispatch routine for the application of known procedures                 */
/* ======================================================================== */

/* Has been cut into three parts to accomodate brainfucked intel processors */

ipointer apply_builtin(ipointer proc,ipointer args) {
   double   xf;
   long int x,y;
   ipointer sv;
   if (special_p(proc)) {
      /* symbol is longer than 3 characters */
      if (proc==car_zap) {
         if (syntaxcheck && (!cbox_p(car(args)) || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: bad args for \"car\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(car(args));
      }
      else if (proc==cdr_zap) {
         if (syntaxcheck && (!cbox_p(car(args)) || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: bad args for \"cdr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(car(args));
      }
      else if (proc==add_zap) {
         x=0;
         while (args!=NIL) {
            if (syntaxcheck && !integer_p(car(args))) {
               printf("SYNTAX-ERROR: illegal argument for \"+\": ");
               write_call(args);
               goto_recoverable_error();
            }
            else {
               x=x+integer_of(car(args));
               args=cdr(args);
            }
         }
         return make_int(x);
      }
      else if (proc==sub_zap) {
         if (syntaxcheck && args==NIL) {
            printf("SYNTAX-ERROR: missing argument for \"-\".");
            goto_recoverable_error();
         }
         if (syntaxcheck && !integer_p(car(args))) {
            printf("SYNTAX-ERROR: illegal argument for \"-\": ");
            write_call(args);
            goto_recoverable_error();
         }
         x=integer_of(car(args));
         if (cdr(args)==NIL) {
            return make_int(-x);
         }
         else {
            args=cdr(args);
            do {
               if (syntaxcheck && !integer_p(car(args))) {
                  printf("SYNTAX-ERROR: illegal argument for \"-\": ");
                  write_call(args);
                  goto_recoverable_error();
               }
               x=x-integer_of(car(args));
               args=cdr(args);
            } while (args!=NIL);
            return make_int(x);
         }
      }
      else if (proc==div_zap) {
         if (syntaxcheck && args==NIL) {
            printf("SYNTAX-ERROR: missing argument for \"/\".");
            goto_recoverable_error();
         }
         if (syntaxcheck && !integer_p(car(args))) {
            printf("SYNTAX-ERROR: illegal argument for \"/\": ");
            write_call(args);
            goto_recoverable_error();
         }
         xf=(double)integer_of(car(args));
         if (cdr(args)==NIL) {
            return make_int((long int)floor(1.0/xf));
         }
         else {
            args=cdr(args);
            do {
               if (syntaxcheck && !integer_p(car(args))) {
                  printf("SYNTAX-ERROR: illegal argument for \"/\": ");
                  write_call(args);
                  goto_recoverable_error();
               }
               xf=xf/(double)integer_of(car(args));
               args=cdr(args);
            } while (args!=NIL);
            xf=floor(xf);
            return make_int((long int)xf);
         }
      }
      else if (proc==mult_zap) {
         x=1;
         while (args!=NIL) {
            if (syntaxcheck && !integer_p(car(args))) {
               printf("SYNTAX-ERROR: illegal argument for \"*\": ");
               write_call(args);
               goto_recoverable_error();
            }
            else {
               x=x*integer_of(car(args));
               args=cdr(args);
            }
         }
         return make_int(x);
      }
      else if (proc==small_zap) {
         if (syntaxcheck && args!=NIL && !integer_p(car(args))) {
            printf("SYNTAX-ERROR: illegal argument for \"<\": ");
            write_call(args);
            goto_recoverable_error();
         }
         if (args==NIL || cdr(args)==NIL) return true_zap;
         x=integer_of(car(args));args=cdr(args);
         do {
            y=x;
            if (syntaxcheck && !integer_p(car(args))) {
               printf("SYNTAX-ERROR: illegal argument for \"<\": ");
               write_call(args);
               goto_recoverable_error();
            }
            x=integer_of(car(args));
            args=cdr(args);
         } while (args!=NIL && y<x);
         if (y<x) return true_zap; else return false_zap;
      }
      else if (proc==smalleq_zap) {
         if (syntaxcheck && args!=NIL && !integer_p(car(args))) {
            printf("SYNTAX-ERROR: illegal argument for \"<=\": ");
            write_call(args);
            goto_recoverable_error();
         }
         if (args==NIL || cdr(args)==NIL) return true_zap;
         x=integer_of(car(args));args=cdr(args);
         do {
            y=x;
            if (syntaxcheck && !integer_p(car(args))) {
               printf("SYNTAX-ERROR: illegal argument for \"<=\": ");
               write_call(args);
               goto_recoverable_error();
            }
            x=integer_of(car(args));
            args=cdr(args);
         } while (args!=NIL && y<=x);
         if (y<=x) return true_zap; else return false_zap;
      }
      else if (proc==eqarith_zap) {
         if (syntaxcheck && args!=NIL && !integer_p(car(args))) {
            printf("SYNTAX-ERROR: illegal argument for \"==\": ");
            write_call(args);
            goto_recoverable_error();
         }
         if (args==NIL || cdr(args)==NIL) return true_zap;
         x=integer_of(car(args));args=cdr(args);
         do {
            y=x;
            if (syntaxcheck && !integer_p(car(args))) {
               printf("SYNTAX-ERROR: illegal argument for \"==\": ");
               write_call(args);
               goto_recoverable_error();
            }
            x=integer_of(car(args));
            args=cdr(args);
         } while (args!=NIL && y==x);
         if (y==x) return true_zap; else return false_zap;
      }
      else if (proc==bigger_zap) {
         if (syntaxcheck && args!=NIL && !integer_p(car(args))) {
            printf("SYNTAX-ERROR: illegal argument for \">\": ");
            write_call(args);
            goto_recoverable_error();
         }
         if (args==NIL || cdr(args)==NIL) return true_zap;
         x=integer_of(car(args));args=cdr(args);
         do {
            y=x;
            if (syntaxcheck && !integer_p(car(args))) {
               printf("SYNTAX-ERROR: illegal argument for \">\": ");
               write_call(args);
               goto_recoverable_error();
            }
            x=integer_of(car(args));
            args=cdr(args);
         } while (args!=NIL && y>x);
         if (y>x) return true_zap; else return false_zap;
      }
      else if (proc==bigeq_zap) {
         if (syntaxcheck && args!=NIL && !integer_p(car(args))) {
            printf("SYNTAX-ERROR: illegal argument for \">=\": ");
            write_call(args);
            goto_recoverable_error();
         }
         if (args==NIL || cdr(args)==NIL) return true_zap;
         x=integer_of(car(args));args=cdr(args);
         do {
            y=x;
            if (syntaxcheck && !integer_p(car(args))) {
               printf("SYNTAX-ERROR: illegal argument for \">=\": ");
               write_call(args);
               goto_recoverable_error();
            }
            x=integer_of(car(args));
            args=cdr(args);
         } while (args!=NIL && y>=x);
         if (y>=x) return true_zap; else return false_zap;
      }
      else if (proc==not_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: illegal argument for \"not\": ");
            write_call(args);
            goto_recoverable_error();
         }
         if (car(args)==false_zap) return true_zap; else return false_zap;
      }
      else if (proc==eqp_zap) {
         if (syntaxcheck && length(args)!=2) {
            printf("SYNTAX-ERROR: illegal args for \"eq?\": " );
            write_call(args);
            goto_recoverable_error();
         }
         return make_bool(equal_p(car(args),car(cdr(args))));
      }
      else {
         printf("Application of unapplicable reserved word ");
         write_call(proc);
         goto_recoverable_error();
      }
   }
   else {
      return apply_builtin1(proc,args);
   }
}

static ipointer apply_builtin1(ipointer proc,ipointer args) {
   double     xf;
   long int   x,y;
   ipointer   sv;
   ringbuffer rb;
      if (proc==cadr_zap) {
         sv=apply_builtin(cdr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cadr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==cdar_zap) {
         sv=apply_builtin(car_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cdar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cddr_zap) {
         sv=apply_builtin(cdr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cddr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==caar_zap) {
         sv=apply_builtin(car_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"caar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==caaar_zap) {
         sv=apply_builtin(caar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"caaar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==caadr_zap) {
         sv=apply_builtin(cadr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"caadr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==cadar_zap) {
         sv=apply_builtin(cdar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cadar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==caddr_zap) {
         sv=apply_builtin(cddr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"caddr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==cdaar_zap) {
         sv=apply_builtin(caar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cdaar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cdadr_zap) {
         sv=apply_builtin(cadr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cdadr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cddar_zap) {
         sv=apply_builtin(cdar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cddar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cdddr_zap) {
         sv=apply_builtin(cddr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cdddr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==caaaar_zap) {
         sv=apply_builtin(caaar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"caaaar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==caaadr_zap) {
         sv=apply_builtin(caadr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"caaadr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==caadar_zap) {
         sv=apply_builtin(cadar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"caadar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==caaddr_zap) {
         sv=apply_builtin(caddr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"caaddr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==cadaar_zap) {
         sv=apply_builtin(cdaar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cadaar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==cadadr_zap) {
         sv=apply_builtin(cdadr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cadadr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==caddar_zap) {
         sv=apply_builtin(cddar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"caddar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);   }
      else if (proc==cadddr_zap) {
         sv=apply_builtin(cdddr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cadddr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return car(sv);
      }
      else if (proc==cdaaar_zap) {
         sv=apply_builtin(caaar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cdaaar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cdaadr_zap) {
         sv=apply_builtin(caadr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cdaadr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cdadar_zap) {
         sv=apply_builtin(cadar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cdadar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cdaddr_zap) {
         sv=apply_builtin(caddr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cdaddr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cddaar_zap) {
         sv=apply_builtin(cdaar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cddaar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cddadr_zap) {
         sv=apply_builtin(cdadr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cddadr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cdddar_zap) {
         sv=apply_builtin(cddar_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cdddar\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==cddddr_zap) {
         sv=apply_builtin(cdddr_zap,args);
         if (syntaxcheck && !cbox_p(sv)) {
            printf("SYNTAX-ERROR: bad args for \"cddddr\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cdr(sv);
      }
      else if (proc==gcstat_zap) {
         if (syntaxcheck && args!=NIL) {
            printf("SYNTAX-ERROR: illegal argument for \"gcstat\": ");
            write_call(args);
            goto_recoverable_error();
         }
         sv=new_cons();
         push_pointer(sv);
         set_car(sv,make_int(stat_lstack_free()));
         sv=new_cons();
         set_cdr(sv,pop_pointer());
         push_pointer(sv);
         set_car(sv,make_int(stat_stack_free()));
         sv=new_cons();
         set_cdr(sv,pop_pointer());
         push_pointer(sv);
         set_car(sv,make_int(stat_storage_free()));
         sv=new_cons();
         set_cdr(sv,pop_pointer());
         push_pointer(sv);
         set_car(sv,make_int(stat_cbox_free()));
         return pop_pointer();
      }
      else if (proc==gcstatwrite_zap) {
         if (syntaxcheck && args!=NIL) {
            printf("SYNTAX-ERROR: illegal argument for \"gcstatwrite\": ");
            write_call(args);
            goto_recoverable_error();
         }
         statistics_mem();
         return NIL;
      }
      else if (proc==synchecktoggle_zap) {
         if (syntaxcheck && args!=NIL) {
            printf("SYNTAX-ERROR: illegal argument for \"synchecktoggle\": ");
            write_call(args);
            goto_recoverable_error();
         }
         syntaxcheck=!syntaxcheck;
         return make_bool(!syntaxcheck);
      }
      else if (proc==cons_zap) {
         if (syntaxcheck && length(args)!=2) {
            printf("SYNTAX-ERROR: illegal argument for \"cons\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return cons(car(args),car(cdr(args)));
      }
      else if (proc==error_zap) {
         if (syntaxcheck && length(args)>1) {
            printf("SYNTAX-ERROR: illegal argument for \"error\": ");
            write_call(args);
            goto_recoverable_error();
         }
         printf("micro-eval error: ");
         if (args!=NIL) write_call(car(args)); else printf("\n");
         goto_recoverable_error();
      }
      else if (proc==integerp_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: illegal argument for \"integer?\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return make_bool(integer_p(car(args)));
      }
      else return apply_builtin2(proc,args);
}

static ipointer apply_builtin2(ipointer proc,ipointer args) {
   double   xf;
   long int x,y;
   ipointer sv;
   status   srs;
      if (proc==length_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL
             || !list_p(car(args)))) {
            printf("SYNTAX-ERROR: illegal argument for \"length\": ");
            write_call(args);
            goto_recoverable_error();
         }
         return make_int((long int)length(car(args)));
      }
      else if (proc==list_zap) {
         return args;
      }
      else if (proc==newline_zap) {
         if (syntaxcheck && args!=NIL) {
            printf("SYNTAX-ERROR: illegal args for \"newline\": ");
            write_call(args);
            goto_recoverable_error();
         }
         printf("\n");
         return NIL;
      }
      else if (proc==nullp_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: illegal args for \"null?\": " );
            write_call(args);
            goto_recoverable_error();
         }
         return make_bool(car(args)==NIL);
      }
      else if (proc==numberp_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: illegal args for \"number?\": " );
            write_call(args);
            goto_recoverable_error();
         }
         return make_bool(number_p(car(args)));
      }
      else if (proc==oddp_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL || !integer_p(car(args)))) {
            printf("SYNTAX-ERROR: illegal args for \"odd?\": " );
            write_call(args);
            goto_recoverable_error();
         }
         return make_bool(!even_p(integer_of(car(args))));
      }
      else if (proc==evenp_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL || !integer_p(car(args)))) {
            printf("SYNTAX-ERROR: illegal args for \"even?\": " );
            write_call(args);
            goto_recoverable_error();
         }
         return make_bool(even_p(integer_of(car(args))));
      }
      else if (proc==pairp_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: illegal args for \"pair?\": " );
            write_call(args);
            goto_recoverable_error();
         }
         return make_bool(cbox_p(car(args)));
      }
      else if (proc==garbagecollect_zap) {
         if (syntaxcheck && args!=NIL) {
            printf("SYNTAX-ERROR: illegal argument for \"garbagecollect\": ");
            write_call(args);
            goto_recoverable_error();
         }
         garbage_collect();
         return NIL;
      }
      else if (proc==stringp_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: illegal args for \"string?\": " );
            write_call(args);
            goto_recoverable_error();
         }
         return make_bool(string_p(car(args)));
      }
      else if (proc==symbolp_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: illegal args for \"symbol?\": " );
            write_call(args);
            goto_recoverable_error();
         }
         return make_bool(symbol_p(car(args)));
      }
      else if (proc==listp_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: illegal args for \"list?\": " );
            write_call(args);
            goto_recoverable_error();
         }
         return make_bool(list_p(car(args)));
      }
      else if (proc==write_zap) {
         if (syntaxcheck && (args==NIL || cdr(args)!=NIL)) {
            printf("SYNTAX-ERROR: illegal args for \"write\": " );
            write_call(args);
            goto_recoverable_error();
         }
         write_call(car(args));
         return NIL;
      }
      else if (proc==read_zap) {
         if (syntaxcheck && cdr(args)!=0) {
            printf("SYNTAX-ERROR: illegal args for \"read\": " );
            write_call(args);
            goto_recoverable_error();
         }
         printf("For later.\n");
         return NIL;
      }
      else if (proc==setcarw_zap) {
         if (syntaxcheck && length(args)!=2) {
            printf("SYNTAX-ERROR: illegal args for \"set-car!\": " );
            write_call(args);
            goto_recoverable_error();
         }
         set_car(car(args),car(cdr(args)));
         return car(args);
      }
      else if (proc==setcdrw_zap) {
         if (syntaxcheck && length(args)!=2) {
            printf("SYNTAX-ERROR: illegal args for \"set-cdr!\": " );
            write_call(args);
            goto_recoverable_error();
         }
         set_cdr(car(args),car(cdr(args)));
         return car(args);
      }
      else {
         printf("Application of unapplicable reserved word ");
         write_call(proc);
         goto_recoverable_error();
      }
}

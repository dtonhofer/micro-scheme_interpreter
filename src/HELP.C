/* ===========================================================================
   Auxiliary procedures
=========================================================================== */

/*{{{  includes --*/
#include <stdlib.h>
#include <stdio.h>
#define NDEBUG
#include <assert.h>
#include "memory.h"
#include "magic.h"
#include "main.h"
#include "help.h"
/*}}}  */

/*{{{  headers of non-exported functions --*/
static ipointer make_frame(ipointer vars,ipointer vals);
static void     set_first_frame_w(ipointer env,ipointer newframe);
/*}}}  */

/*{{{  check whether an ulong is even --*/
bool even_p(ulong val) {
   return (val &0x01)==0;

}
/*}}}  */

/*{{{  return printable character --*/
char printit(char ch) {
   if ((ch>=0 && ch<32) || ch==127) return '-';
   else return ch;
}
/*}}}  */

/* environment and frame manipulation ===================================== */

/*{{{  return first (uppermost) frame of an environment --*/
ipointer first_frame(ipointer cur) {
   assert(cbox_p(cur) && hint_environment_p(cur));
   return cdr(cur);
}
/*}}}  */

/*{{{  return parent environment of an environment --*/
ipointer parent(ipointer cur) {
   assert(cbox_p(cur) && hint_environment_p(cur));
   return car(cur);
}
/*}}}  */

/*{{{  return a pointer to the starting environment --*/
ipointer create_begin_env(void) {
   ipointer p1,p2,be;
   be=new_cons();
   push_pointer(be);
   p1=new_cons();set_cdr(be,p1);
   p2=new_cons();set_car(p1,p2);
   set_car(p2,make_symbol("!!"));
   set_cdr(p2,make_string("Written by D.T. 1993"));
   p2=new_cons();set_cdr(p1,p2);p1=p2;
   p2=new_cons();set_car(p1,p2);
   set_car(p2,make_symbol("begin_env"));
   set_cdr(p2,be);
   set_hint_environment(be);
   pop_pointer();
   return be;
}
/*}}}  */

/*{{{  retrieve a binding from a frame --*/
ipointer binding_in_frame(ipointer var,ipointer frame) {
   ipointer p1=NIL;
   while (frame!=NIL && p1==NIL) {
      if (equal_p(var,binding_variable(first_binding(frame)))) {
         p1=first_binding(frame);
      }
      else {
      frame=rest_bindings(frame);
      }
   }
   return p1;
}
/*}}}  */

/*{{{  retrieve a binding from an environment --*/
/* Search for a binding within an environment */
ipointer binding_in_env(ipointer var,ipointer env) {
   ipointer p1=NIL;
   while (env!=NIL && p1==NIL) {
      p1=binding_in_frame(var,first_frame(env));
      env=parent(env);
   }
   return p1;
}
/*}}}  */

/*{{{  return first binding of a frame --*/
ipointer first_binding(ipointer cur) {
   assert(cbox_p(cur));
   return car(cur);
}
/*}}}  */

/*{{{  return list of remaining bindings of a frame --*/
ipointer rest_bindings(ipointer cur) {
   assert(cbox_p(cur));
   return cdr(cur);
}
/*}}}  */

/*{{{  create a frame from a list of vars and a list of vals --*/
/* "goto_recoverable_error()" will be called if an error occured */
ipointer make_frame(ipointer vars,ipointer vals) {
   ipointer p,end;
   assert(list_p(vals) && symbol_compound_p(vars));
   if (symbol_p(vars)) {
      p=cons(vars,vals);push_pointer(p);
      p=adjoin_binding(p,NIL);
      pop_pointer();
   }
   else if (cbox_p(vars) && cbox_p(vals)) {
      p=cons(car(vars),car(vals));
      vars=cdr(vars);vals=cdr(vals);
      push_pointer(p);
      p=cons(p,NIL);end=p;
      pop_pointer();
      push_pointer(p);
      while (cbox_p(vars) && cbox_p(vals)) {
         set_cdr(end,cons(NIL,NIL));
         end=cdr(end);
         set_car(end,cons(car(vars),car(vals)));
         vals=cdr(vals);vars=cdr(vars);
      }
      if (symbol_p(vars)) {
         set_cdr(end,cons(NIL,NIL));
         end=cdr(end);
         set_car(end,cons(vars,vals));
      }
      else if (vars!=NIL || vals!=NIL) {
         printf("RUNTIME-ERROR: mismatch during make-frame().\n");
         printf("   Variables are: ");write_call(vars);
         printf("   Values    are: ");write_call(vals);
         goto_recoverable_error();
      }
      pop_pointer();
   }
   else {
      printf("RUNTIME-ERROR: problem arose during make-frame().\n");
      printf("   Variables are: ");write_call(vars);
      printf("   Values    are: ");write_call(vals);
      goto_recoverable_error();
   }
   return p;
}
/*}}}  */

/*{{{  insert a new variable into the topmost frame --*/
/* No check is made as to whether this operation is meaningful */
void define_variable_w(ipointer var,ipointer val,ipointer env) {
   ipointer p;
   assert(cbox_p(env) && hint_environment_p(env));
   p=cons(var,val);
   push_pointer(p);
   p=adjoin_binding(p,first_frame(env));
   pop_pointer();
   set_first_frame_w(env,p);
   set_hint_environment(env);
}
/*}}}  */

/*{{{  modify destructively the value of a variable in an environment --*/
/* "goto_recoverable_error()" is called if the operation fails */
void set_variable_w(ipointer var,ipointer val,ipointer env) {
   ipointer p;
   assert(cbox_p(env) && hint_environment_p(env) && symbol_p(var));
   p=binding_in_env(var,env);
   if (p==NIL) {
      printf("RUNTIME-ERROR: unable to modify undefined variable!\n");
      write_call(var);
      goto_recoverable_error();
   }
   else {
      set_cdr(p,val);
   }
}
/*}}}  */

/*{{{  add a new frame to the environment given and return it --*/
/* this procedure may receive NIL variables or NIL values */
ipointer extend_environment(ipointer vars,ipointer vals,ipointer base_env) {
   ipointer p1;
   assert(cbox_p(base_env) && hint_environment_p(base_env));
   if (vars==NIL && vals==NIL) {
      p1=base_env;
   }
   else {
      p1=make_frame(vars,vals);
      push_pointer(p1);
      p1=cons(base_env,p1);
      set_hint_environment(p1);
      pop_pointer();
   }
   return p1;
}
/*}}}  */

/*{{{  destructively set the frame of an environment --*/
void set_first_frame_w(ipointer env,ipointer newframe) {
   assert(cbox_p(env) && hint_environment_p(env));
   set_cdr(env,newframe);
}
/*}}}  */

/* procedure manipulation ================================================= */

/*{{{  return environment of a procedure --*/
ipointer proc_env(ipointer cur) {
   assert(cbox_p(cur) && hint_procedure_p(cur));
   return cdr(cur);
}
/*}}}  */

/*{{{  return text of a procedure --*/
ipointer proc_text(ipointer cur) {
   assert(cbox_p(cur) && hint_procedure_p(cur));
   return car(cur);
}
/*}}}  */

/*{{{  return the body of a (compound) procedure --*/
ipointer proc_body(ipointer cur) {
   assert(cbox_p(cur) && hint_procedure_p(cur));
   assert(cbox_p(car(cur)));
   return cdr(cdr(car(cur)));
}
/*}}}  */

/*{{{  return the variables of a procedure (might be symbol only) --*/
ipointer proc_params(ipointer cur) {
   assert(cbox_p(cur) && hint_procedure_p(cur));
   assert(cbox_p(car(cur)));
   return car(cdr(car(cur)));
}
/*}}}  */

/* syntax checks ========================================================== */

/*{{{  check whether a given argument list has no duplicate symbols --*/
bool unique_vars_p(ipointer vars) {
   ipointer cur,x;
   bool     res=TRUE;
   assert(symbol_compound_p(vars));
   if (cbox_p(vars)) {
      while (cbox_p(vars) && res) {
         x=car(vars);
         cur=cdr(vars);
         vars=cur;
         while (cbox_p(cur) && !equal_p(x,car(cur))) cur=cdr(cur);
         if (symbol_p(cur)) {
            res=!equal_p(x,cur);
         }
         else if (cbox_p(cur)) res=FALSE;
      }
   }
   return res;
}
/*}}}  */

/*{{{  check whether a given pointer "is" a list composed of symbols only --*/
bool symbol_list_p(ipointer cur) {
   bool res=TRUE;
   while (res && cur!=NIL) {
      if (cbox_p(cur) && symbol_p(car(cur))) cur=cdr(cur); else res=FALSE;
   }
   return res;
}
/*}}}  */

/*{{{  check for chain of symbols, maybe terminated with a symbol --*/
bool symbol_compound_p(ipointer cur) {
   bool res=TRUE;
   while (res && cur!=NIL) {
      if (cbox_p(cur) && symbol_p(car(cur))) cur=cdr(cur); else res=FALSE;
   }
   return (res || symbol_p(cur));
}
/*}}}  */

/*{{{  check if the "cond"-clauses are well-formed --*/
bool list_of_clauses_p(ipointer cur) {
   bool res=TRUE;
   int  len=0;
   while (res==TRUE && cur!=NIL) {
      if (cbox_p(cur) && car(cur)!=NIL && list_p(car(cur)) &&
          (car(car(cur))!=else_zap ||
          (cdr(cur)==NIL && len!=0 && length(car(cur))>=2))) {
         cur=cdr(cur);len++;
      }
      else res=FALSE;
   }
   return res;
}
/*}}}  */

/*{{{  check whether the second argument of a "let" is well-formed --*/
bool assoc_list_p(ipointer cur) {
   bool     res=TRUE;
   ipointer a;
   while (cur!=NIL && res) {
      if (cbox_p(cur)) {
         a=car(cur);
         if (cbox_p(a) && symbol_p(car(a))) {
            a=cdr(a);
            if (cbox_p(a) && cdr(a)==NIL) cur=cdr(cur);
            else res=FALSE;
         }
         else res=FALSE;
      }
      else res=FALSE;
   }
   return res;
}
/*}}}  */

/*{{{  check whether a given pointer "is" a list (NIL is a list) --*/
bool list_p(ipointer cur) {
   bool res=TRUE;
   while (res && cur!=NIL) {
      if (cbox_p(cur)) cur=cdr(cur); else res=FALSE;
   }
   return res;
}
/*}}}  */

/* syntax transformations ================================================= */

/*{{{  divide an association list into (var-list.val-list) --*/
ipointer separate_assoc(ipointer list) {
   ipointer var,varlast,val,vallast,asc;
   if (list!=NIL) {
      asc=car(list);
      var=new_cons();set_cdr(var,NIL);set_car(var,car(asc));
      push_pointer(var);varlast=var;
      val=new_cons();set_cdr(val,NIL);set_car(val,car(cdr(asc)));
      push_pointer(val);vallast=val;
      list=cdr(list);
      while (list!=NIL) {
         asc=car(list);
         set_cdr(varlast,new_cons());
         varlast=cdr(varlast);
         set_cdr(varlast,NIL);set_car(varlast,car(asc));
         set_cdr(vallast,new_cons());
         vallast=cdr(vallast);
         set_cdr(vallast,NIL);set_car(vallast,car(cdr(asc)));
         list=cdr(list);
      }
   }
   else {
      push_pointer(NIL);push_pointer(NIL);
   }
   asc=new_cons();
   set_cdr(asc,pop_pointer());
   set_car(asc,pop_pointer());
   return asc;
}
/*}}}  */

/*{{{  extract clauses of conditional expression --*/
ipointer clauses(ipointer expr) {
   ipointer p;
   if (operator(expr)==if_zap) {
      /* if-then */
      p=new_cons();set_cdr(p,NIL);set_car(p,second_arg(expr));
      push_pointer(p);
      p=new_cons();set_cdr(p,pop_pointer());set_car(p,first_arg(expr));
      push_pointer(p);
      p=NIL;
      if (length(expr)==4) {
         /* if-then-else */
         p=new_cons();set_cdr(p,NIL);set_car(p,third_arg(expr));
         push_pointer(p);
         p=new_cons();set_cdr(p,pop_pointer());set_car(p,else_zap);
         push_pointer(p);
         p=new_cons();
         set_cdr(p,NIL);set_car(p,pop_pointer());
      }
      push_pointer(p);
      p=new_cons();
      set_cdr(p,pop_pointer());set_car(p,pop_pointer());
   }
   else {
      p=operands(expr); /* should be a well-formed cond */
   }
   return p;
}
/*}}}  */

/* expression manipulation ================================================ */

/*{{{  return the operator of an s-expression --*/
ipointer operator(ipointer cur) {
   assert(cbox_p(cur));
   return car(cur);
}
/*}}}  */

/*{{{  return the operands of an s-expression --*/
ipointer operands(ipointer cur) {
   assert(cbox_p(cur));
   return cdr(cur);
}
/*}}}  */

/*{{{  return the first argument of an s-expression --*/
ipointer first_arg(ipointer cur) {
   assert(cbox_p(cur) && cbox_p(cdr(cur)));
   return car(cdr(cur));
}
/*}}}  */

/*{{{  return the second argument of an s-expression --*/
ipointer second_arg(ipointer cur) {
   assert(cbox_p(cur) && cbox_p(cdr(cur)) && cbox_p(cdr(cdr(cur))));
   return car(cdr(cdr(cur)));
}
/*}}}  */

/*{{{  return the third argument of an s-expression --*/
ipointer third_arg(ipointer cur) {
   assert(cbox_p(cur) && cbox_p(cdr(cur)) && cbox_p(cdr(cdr(cur))) &&
          cbox_p(cdr(cdr(cdr(cur)))));
   return car(cdr(cdr(cdr(cur))));
}
/*}}}  */

/*{{{  return the length of a list --*/
int length(ipointer cur) {
   int i=0;
   assert(list_p(cur));
   while (cur!=NIL) {
      i++;cur=cdr(cur);
   }
   return i;
}
/*}}}  */

/*{{{  cons two elements --*/
/* the parameters must be GC-accesible */
ipointer cons(ipointer a,ipointer b) {
   ipointer p1;
   p1=new_cons();
   set_car(p1,a);
   set_cdr(p1,b);
   return p1;
}
/*}}}  */

/* bindings =============================================================== */

/*{{{  add a binding to a frame, return modified frame --*/
/* the parameters must be GC-accessible */
ipointer adjoin_binding(ipointer binding,ipointer frame) {
   ipointer p1;
   p1=new_cons();
   set_cdr(p1,frame);
   set_car(p1,binding);
   return p1;
}
/*}}}  */

/*{{{  get the variable of a binding --*/
ipointer binding_variable(ipointer cur) {
   assert(cbox_p(cur));
   return car(cur);
}
/*}}}  */

/*{{{  get the value of a binding --*/
ipointer binding_value(ipointer cur) {
   assert(cbox_p(cur));
   return cdr(cur);
}
/*}}}  */

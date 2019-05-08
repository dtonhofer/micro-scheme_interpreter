/* ===========================================================================
   Main module
   -----------
   The main module contains the code for the evaluation of an expression. In
   detail:

   Main procedure
   --------------
   The main procedure evaluates any files that have been passed as arguments.
   As soon as this has been done, the standard input is used as input to
   micro-eval. After termination of micro-eval, the program exits. Notice
   that the "startup-environment", "begin_env" never changes during program
   execution; only its frame may be switched. Also, it is accessible at all
   times to the garbage collector, as it has been put onto the reverse stack.

   Micro-eval
   ----------
   This is the read-eval-print loop. It runs until the parser gives an EOF
   message. Also, it loads the (globally visible) "scheme registers" "env"
   and "exp" with the begin-environment and the parser output respectively.
   Micro-eval expects the evaluation result in "val".

   Evaluation loop
   ---------------
   If the loop is called, the stacks are empty, the registers are set to
   NIL except for "exp" which holds the (pointer to) the expression to be
   evaluated, and "env" which contains a pointer to the environment.
   The first that is done is to put an "end-label" on the stack. Then the
   loop is entered.
   Upon entering the loop (with START_LABEL), the following must hold:
   - "cont" contains the procedure label of the procedure to run.
   - the first element on the label stack is the "return label" to which
     the loop will branch if it is done with the current expression.
   - "exp" and "env" contain valid pointers.
   If the loop is entered with another label than START_LABEL, special
   conditions must be met (e.g. data must have been stored on-stack). This
   is indicated on a per-label basis.
   If the loop is done, the evaluation result can be found in "val".
   The "calling" conventions are as follows:

   JUMPing to a label     CALLING a label           RETURNing to caller
   ------------------     ---------------           -------------------
   cont_reg:=LABEL        push_label(RETURN_LABEL)  cont_reg=pop_label();
   break                  cont_reg:=LABEL           break;
                          break

   Application
   -----------
   The application of a function to a list of arguments is done by the second
   part of the evaluation loop. If there are no arguments, "micro-apply" is
   called directly; if there is but one argument, it is evaluated, then
   "micro-apply" is called. If there are several arguments, these are
   evaluated in a tight loop one after another and dumped on the stack.
   Track is kept of the number of evaluated arguments by pushing enough
   labels onto the label stack to allow a later procedure that collects
   the elements from the stack and puts them into a list, to loop as long as
   there are evaluated arguments left on the pointer stack.

   Error recovery
   --------------
   If an error occurs during push, pop or allocation, or program execution,
   "goto_recoverable_error()" may be called. This will cause a longjump
   (using "jump_environment") to an error recovery routine. This routine
   is called "set_terminal_error" (it will exit()) during initialization,
   and "set_recoverable_error()" (it will clear the stacks) during evaluation.
   The semantics of "longjump" are such that calling the jump will switch
   the execution flow to the error recovery routine, as if it had never been
   left.

=========================================================================== */

/*{{{  includes --*/
#include <stdio.h>
#include <stdlib.h>
#define NDEBUG
#include <assert.h>
#include <setjmp.h>
#include <math.h>
#include "memory.h"
#include "magic.h"
#include "parser.h"
#include "help.h"
#include "main.h"
#include "builtin.h"
/*}}}  */

/*{{{  labels for evaluation loop --*/
#define START_LABEL                          0
#define SELF_EVAL_P_LABEL                    1
#define VARIABLE_P_LABEL                     2
#define FORGET_ABOUT_IT_LABEL                3
#define QUOTED_P_LABEL                       4
#define SP_DEFINITION_P_LABEL                5
#define LET_P_LABEL                          6
#define AND_P_LABEL                          7
#define OR_P_LABEL                           8
#define ASSIGNMENT_P_LABEL                   9
#define CONDITIONAL_P_LABEL                  10
#define LAMBDA_P_LABEL                       11
#define APPLICATION_P_LABEL                  12
#define UNKNOWN_EXPR_LABEL                   13
#define LIST_OF_VALUES_LABEL                 14
#define LIST_OF_VALUES_CONT_LABEL            15
#define LIST_OF_VALUES_COLLECT_START_LABEL   16
#define LIST_OF_VALUES_COLLECT_LABEL         17
#define LIST_OF_VALUES_COLLECT_STOP_LABEL    18
#define MICRO_APPLY_LABEL                    19
#define DEFINITION_CONT_LABEL                20
#define AND_CONT_LABEL                       21
#define OR_CONT_LABEL                        22
#define ASSIGNMENT_CONT_LABEL                23
#define CONDITIONAL_CONT_LABEL               24
#define EVAL_SEQUENCE_LABEL                  25
#define EVAL_SEQUENCE_CONT_LABEL             26
#define ERROR_LABEL                          27
#define END_LABEL                            28
/*}}}  */

/*{{{  procedure headers --*/
static void     micro_eval(ringbuffer rb,ipointer begin_env);
extern int      main(int argc,char *argv[]);
static void     evaluation_loop(void);
/*}}}  */

/*{{{  global variables --*/
static jmp_buf jump_environment;   /* The current recovery environment */
bool   syntaxcheck;
/*}}}  */

/*{{{  procedure to be called on error --*/
void goto_recoverable_error(void) {
   longjmp(jump_environment,1);
   /* execution passes to error routine */
}
/*}}}  */

/* ======================================================================== */
/* Here comes the beef                                                      */
/* ======================================================================== */

/*{{{  main procedure --*/
int main(int argc,char *argv[]) {
   FILE           *infile;
   ipointer       begin_env;
   ringbuffer     rb;
   int            i;

   /* Initializations */

   if (setjmp(jump_environment)!=0) {
      /* just returned from an error */
      printf("Bailing out.\n");
      cleanup_mem();
      exit(1);
   }
   else {
      /* just set up longjump */
      init_mem();init_magic();
      begin_env=create_begin_env();
      revpush_pointer(begin_env);
   }

   /* If files specified, evaluate them */

   for (i=1;i<argc;i++) {
      infile=fopen(argv[i],"r");
      if (infile==NULL) {
         printf("STARTUP-ERROR: couldn't open file \"%s\".\n",argv[i]);
      }
      else {
         printf("Reading from file \"%s\".\n",argv[i]);
         rb=new_ringbuffer(infile);
         if (rb==NULL) {
            printf("STARTUP-ERROR: couldn't allocate input buffer.\n");
         }
         else {
            micro_eval(rb,begin_env);
            release_ringbuffer(rb); /* File is closed automatically */
            printf("End for file \"%s\".\n",argv[i]);
         }
      }
   }
   printf("Reading from stdin.\n");
   rb=new_ringbuffer(stdin);
   if (rb==NULL) {
      printf("STARTUP-ERROR: couldn't allocate input buffer.\n");
   }
   micro_eval(rb,begin_env);
   release_ringbuffer(rb);
   printf("Morituri te salutant.\n");
   cleanup_mem();
   return 0;
}
/*}}}  */

/*{{{  read-eval-print loop --*/
void micro_eval(ringbuffer rb,ipointer begin_env) {
   bool stop=FALSE,srs;
   syntaxcheck=TRUE;
   do {
      if (setjmp(jump_environment)!=0) {
         /* just returned from an error */
         printf("Resetting interpreter.\n");
         init_stack();init_registers();
         garbage_collect();
      }
      else {
         /* we have just set the jump */
         do {
            printf("Micro-eval => ");
            init_registers();
            exp_reg=read_call(rb,(status *)(&srs));
            env_reg=begin_env;
            switch (srs) {
               case ERROR: break;
               case TERM:  stop=TRUE;
                           break;
               case STOP:  stop=TRUE;
                           /* Fall-through */
               case OK:    printf("Evaluating...\n");
                           evaluation_loop();
                           write_call(val_reg);
                           set_variable_w(make_symbol("!!"),val_reg,begin_env);
                           break;
               default:    printf("PROGRAM ERROR: unknown parser response.\n");
            }
            /* the stacks must be empty */
            assert(stat_stack_free()==STACKD);
            assert(stat_lstack_free()==LSTACKD);
         } while (!stop);
      }
   }  while (!stop);
}
/*}}}  */

/*{{{  the evaluation loop --*/
static void evaluation_loop(void) {
   ipointer oper;
   assert(cbox_p(env_reg));
   assert(stat_stack_free()==STACKD);
   assert(stat_lstack_free()==LSTACKD);
   push_label(END_LABEL);
   cont_reg=START_LABEL;
   do {
      switch (cont_reg) {

      /*{{{  first level analysis and dispatching --*/
      
      case START_LABEL:
      
         /*{{{  dispatch depending on whether it's a cbox or not --*/
         /* registers:exp,env contain meaningful values */
         if (cbox_p(exp_reg)) {
            oper=operator(exp_reg);
            cont_reg=QUOTED_P_LABEL;
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case SELF_EVAL_P_LABEL:
      
         /*{{{  is exp self-evaluating ? --*/
         /* registers:exp,env contain meaningful values */
         if (number_p(exp_reg) || bool_p(exp_reg) || exp_reg==NIL || string_p(exp_reg)
            || char_p(exp_reg)) {
            val_reg=exp_reg;
            cont_reg=pop_label();
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case VARIABLE_P_LABEL:
      
         /*{{{  is exp a variable ? --*/
         /* registers:exp,env contain meaningful values */
         if (symbol_p(exp_reg)) {
            if (reserved_p(exp_reg)) {
               /* It's a reserved symbol... */
               /* COULD be a built-in procedure, so let's create one... */
               val_reg=new_cons();
               set_car(val_reg,exp_reg); /* exp is already the "key" */
               set_hint_procedure(val_reg);
               cont_reg=pop_label();
            }
            else {
               val_reg=binding_in_env(exp_reg,env_reg);
               if (val_reg==NIL) {
                  printf("RUNTIME ERROR: unbound variable ");
                  write_call(exp_reg);
                  cont_reg=ERROR_LABEL;
               }
               else {
                  val_reg=binding_value(val_reg);
                  cont_reg=pop_label();
               }
            }
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case FORGET_ABOUT_IT_LABEL:
      
         /*{{{  it's no cbox, so it's unknown --*/
         assert(!cbox_p(exp_reg));
         cont_reg=UNKNOWN_EXPR_LABEL;
         break;
         /*}}}  */
      
      case QUOTED_P_LABEL:
      
         /*{{{  is exp quoted ? --*/
         /* registers:exp,env contain meaningful values */
         if (oper==quote_zap) {
            if (syntaxcheck && (!list_p(exp_reg) || length(exp_reg)!=2)) {
               printf("SYNTAX ERROR: incorrect usage for \"quote\" in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            val_reg=first_arg(exp_reg);
            cont_reg=pop_label();
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case SP_DEFINITION_P_LABEL:
      
         /*{{{  is exp a definition ? --*/
         /* registers:exp,env contain meaningful values */
         if (oper==define_zap) {
            if (syntaxcheck && (!list_p(exp_reg) || length(exp_reg)<3)) {
               printf("SYNTAX ERROR: incorrect usage for \"define\" in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            if (symbol_list_p(first_arg(exp_reg))) {
               /* sugared "define lambda": Transform into "define" */
               val_reg=new_cons();
               set_cdr(val_reg,cdr(operands(exp_reg)));
               set_car(val_reg,cdr(first_arg(exp_reg)));
               push_pointer(val_reg);
               val_reg=new_cons();
               set_car(val_reg,lambda_zap);
               set_cdr(val_reg,pop_pointer());
               push_pointer(val_reg);
               val_reg=new_cons();
               set_car(val_reg,pop_pointer());
               push_pointer(val_reg);
               val_reg=new_cons();
               set_car(val_reg,car(first_arg(exp_reg)));
               set_cdr(val_reg,pop_pointer());
               push_pointer(val_reg);
               exp_reg=new_cons();
               set_car(exp_reg,define_zap);
               set_cdr(exp_reg,pop_pointer());
            }
            /* evaluate "define" */
            if (syntaxcheck && (length(exp_reg)!=3 || !symbol_p(first_arg(exp_reg)))) {
               printf("SYNTAX ERROR: incorrect usage for \"define\" in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            if (reserved_p(first_arg(exp_reg))) {
               printf("RUNTIME ERROR: attempt to \"define\" a keyword in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            /* check if value exists already */
            val_reg=binding_in_frame(first_arg(exp_reg),first_frame(env_reg));
            if (val_reg!=NIL) {
               printf("WARNING: overwriting previous definition in ");
               write_call(exp_reg);
            }
            push_pointer(env_reg);
            push_pointer(val_reg);
            push_pointer(first_arg(exp_reg));
            /* evaluate the definition's argument */
            push_label(DEFINITION_CONT_LABEL);
            exp_reg=second_arg(exp_reg);
            cont_reg=START_LABEL;
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case LET_P_LABEL:
      
         /*{{{  is exp a "let" ? --*/
         /* registers:exp,env contain meaningful values */
         if (oper==let_zap) {
            if (syntaxcheck && (!list_p(exp_reg) ||
                length(exp_reg)<3 || !assoc_list_p(first_arg(exp_reg)))) {
               printf("SYNTAX ERROR: incorrect usage for \"let\" in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            /* translate the let into a "lambda" */
            argl_reg=separate_assoc(first_arg(exp_reg));
            val_reg=new_cons();
            set_cdr(val_reg,cdr(operands(exp_reg)));
            set_car(val_reg,car(argl_reg));
            push_pointer(val_reg);
            val_reg=new_cons();
            set_car(val_reg,lambda_zap);
            set_cdr(val_reg,pop_pointer());
            push_pointer(val_reg);
            exp_reg=new_cons();
            set_car(exp_reg,pop_pointer());
            set_cdr(exp_reg,cdr(argl_reg));
            /* evaluate, but don't return here */
            cont_reg=APPLICATION_P_LABEL;
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case AND_P_LABEL:
      
         /*{{{  is exp an "and" ? --*/
         /* registers:exp,env contain meaningful values */
         if (oper==and_zap) {
            if (syntaxcheck && !list_p(exp_reg)) {
               printf("SYNTAX ERROR: incorrect usage for \"and\" in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            exp_reg=operands(exp_reg);
            if (exp_reg==NIL) {
               /* null "and" */
               val_reg=true_zap;
               cont_reg=pop_label();
            }
            else {
               if (cdr(exp_reg)!=NIL) {
                  /* more than one argument */
                  push_label(AND_CONT_LABEL);
                  push_pointer(env_reg);
                  push_pointer(cdr(exp_reg));
               }
               /* evaluate first argument */
               exp_reg=car(exp_reg);
               cont_reg=START_LABEL;
            }
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case OR_P_LABEL:
      
         /*{{{  is exp an "or" ? --*/
         /* registers:exp,env contain meaningful values */
         if (oper==or_zap) {
            if (syntaxcheck && !list_p(exp_reg)) {
               printf("SYNTAX ERROR: incorrect usage for \"or\" in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            exp_reg=operands(exp_reg);
            if (exp_reg==NIL) {
               /* null "or" */
               val_reg=false_zap;
               cont_reg=pop_label();
            }
            else {
               if (cdr(exp_reg)!=NIL) {
                  push_label(OR_CONT_LABEL);
                  push_pointer(env_reg);
                  push_pointer(cdr(exp_reg));
               }
               exp_reg=car(exp_reg);
               cont_reg=START_LABEL;
            }
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case ASSIGNMENT_P_LABEL:
      
         /*{{{  is exp a "set!" ? --*/
         /* registers:exp,env contain meaningful values */
         if (oper==setw_zap) {
            if (syntaxcheck && (!list_p(exp_reg) || length(exp_reg)!=3 ||
                !symbol_p(first_arg(exp_reg)))) {
               printf("SYNTAX ERROR: incorrect usage for \"set!\" in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            if (reserved_p(first_arg(exp_reg))) {
               printf("RUNTIME ERROR: attempt to \"set!\" a keyword in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            val_reg=binding_in_env(first_arg(exp_reg),env_reg);
            if (val_reg==NIL) {
               printf("RUNTIME ERROR: unable to \"set!\" undefined variable in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            push_pointer(env_reg);
            push_pointer(val_reg);
            push_pointer(first_arg(exp_reg));
            push_label(ASSIGNMENT_CONT_LABEL);
            exp_reg=second_arg(exp_reg);
            cont_reg=START_LABEL;
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case CONDITIONAL_P_LABEL:
      
         /*{{{  is exp an "if" or a "cond" ? --*/
         /* registers:exp,env contain meaningful values */
         if (oper==if_zap || oper==cond_zap) {
            if (syntaxcheck && !(list_p(exp_reg) &&
               ((car(exp_reg)==if_zap && (length(exp_reg)==3 || length(exp_reg)==4)) ||
               (car(exp_reg)==cond_zap && length(exp_reg)>=2 &&
                list_of_clauses_p(operands(exp_reg)))))) {
               printf("SYNTAX ERROR: incorrect usage for conditional in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            push_pointer(exp_reg);
            exp_reg=clauses(exp_reg);
            push_pointer(env_reg);
            push_pointer(cdr(exp_reg));
            push_label(CONDITIONAL_CONT_LABEL);
            exp_reg=car(exp_reg);
            push_pointer(cdr(exp_reg));
            exp_reg=car(exp_reg);
            cont_reg=START_LABEL;
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case LAMBDA_P_LABEL:
      
         /*{{{  is exp a "lambda" ? --*/
         /* registers:exp,env contain meaningful values */
         if (oper==lambda_zap) {
            if (syntaxcheck && (!list_p(exp_reg) || length(exp_reg)<3 ||
                !symbol_compound_p(first_arg(exp_reg)) ||
                !unique_vars_p(first_arg(exp_reg)))) {
               printf("SYNTAX ERROR: incorrect usage for \"lambda\" in ");
               write_call(exp_reg);
               cont_reg=ERROR_LABEL;
               break;
            }
            /* create a compound procedure */
            val_reg=new_cons();
            set_car(val_reg,exp_reg);
            set_cdr(val_reg,env_reg);
            set_hint_procedure(val_reg);
            cont_reg=pop_label();
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case APPLICATION_P_LABEL:
      
         /*{{{  is exp an application (fun x1...xn) ? --*/
         /* registers:exp,env contain meaningful values */
         if (!syntaxcheck || list_p(exp_reg)) {
            /* evaluate the operator first */
            push_pointer(env_reg);
            push_pointer(operands(exp_reg));
            push_label(LIST_OF_VALUES_LABEL);
            exp_reg=car(exp_reg);
            cont_reg=START_LABEL;
            break;
         }
         /* Fall-through */
         /*}}}  */
      
      case UNKNOWN_EXPR_LABEL:
      
         /*{{{  this is the end ? --*/
         printf("RUNTIME ERROR: unknown expression ");
         write_call(exp_reg);
         cont_reg=ERROR_LABEL;
         break;
         /*}}}  */
      
      /*}}}  */

      /*{{{  normal order evaluation and application --*/
      
      case LIST_OF_VALUES_LABEL:
      
         /*{{{  start of argument evaluation --*/
         /* registers: val contains function to apply */
         /* stack: 1.list of unevaluated operands, 2.environment */
         exp_reg=pop_pointer();
         env_reg=pop_pointer();
         fun_reg=val_reg;
         if (syntaxcheck && !hint_procedure_p(fun_reg)) {
            printf("RUNTIME-ERROR: application of unapplicable schmilblik ");
            write_call(fun_reg);
            cont_reg=ERROR_LABEL;
            break;
         }
         if (exp_reg==NIL) {
            /* no arguments */
            argl_reg=NIL;
            cont_reg=MICRO_APPLY_LABEL;
            break;
         }
         else {
            push_pointer(fun_reg);
            push_label(LIST_OF_VALUES_COLLECT_STOP_LABEL);
            if (cdr(exp_reg)!=NIL) {
               /* more than one argument */
               push_label(LIST_OF_VALUES_CONT_LABEL);
               push_pointer(env_reg);
               push_pointer(cdr(exp_reg));
            }
            else {
               /* only one argument */
               push_label(LIST_OF_VALUES_COLLECT_START_LABEL);
            }
            /* evaluate first argument */
            exp_reg=car(exp_reg);
            cont_reg=START_LABEL;
            break;
         }
         /*}}}  */
      
      case LIST_OF_VALUES_CONT_LABEL:
      
         /*{{{  loop for the evaluation of the arguments in order --*/
         /* registers: val contains result of argument evaluation */
         /* stack: 1.rest of unevaluated operands, 2.environment */
         exp_reg=pop_pointer();
         env_reg=pop_pointer();
         push_pointer(val_reg); /* push arg result on stack */
         push_label(LIST_OF_VALUES_COLLECT_LABEL);
         if (cdr(exp_reg)==NIL) {
            /* only one argument to go */
            push_label(LIST_OF_VALUES_COLLECT_START_LABEL);
         }
         else {
            /* several arguments */
            push_label(LIST_OF_VALUES_CONT_LABEL);
            push_pointer(env_reg);
            push_pointer(cdr(exp_reg));
         }
         /* evaluate first argument */
         exp_reg=car(exp_reg);
         cont_reg=START_LABEL;
         break;
         /*}}}  */
      
      case LIST_OF_VALUES_COLLECT_START_LABEL:
      
         /*{{{  push last evaluated argument, ready for collection --*/
         /* registers: val contains value of last argument evaluation */
         /* stack: contains evaluated arguments in reverse order      */
         argl_reg=NULL;
         push_pointer(val_reg);
         /*}}}  */
      
      case LIST_OF_VALUES_COLLECT_LABEL:
      
         /*{{{  loop, collecting the arguments in argl --*/
         /* the stack is filled with evaluated arguments */
         unev_reg=new_cons();
         set_cdr(unev_reg,argl_reg);
         set_car(unev_reg,pop_pointer());
         argl_reg=unev_reg;
         cont_reg=pop_label();
         break;
         /*}}}  */
      
      case LIST_OF_VALUES_COLLECT_STOP_LABEL:
      
         /*{{{  load function --*/
         /* registers: argl contains list of arguments */
         /* stack: 1.function to apply */
         fun_reg=pop_pointer();
         /* fall-through */
         /*}}}  */
      
      case MICRO_APPLY_LABEL:
      
         /*{{{  application of function to arguments --*/
         /* registers: fun contains function, argl contains arguments */
         if (cdr(fun_reg)==NIL) {
            /* built-in function (maybe a function) */
            val_reg=apply_builtin(car(fun_reg),argl_reg);
            cont_reg=pop_label();
            break;
         }
         else {
            /* compound procedure */
            env_reg=extend_environment(proc_params(fun_reg),argl_reg,proc_env(fun_reg));
            exp_reg=proc_body(fun_reg);
            cont_reg=EVAL_SEQUENCE_LABEL;
            break;
         }
         /*}}}  */
      
      /*}}}  */

      /*{{{  assorted auxiliairies --*/
      
      case DEFINITION_CONT_LABEL:
      
         /*{{{  second part of definition --*/
         /* registers: val contains 2nd argument of definition */
         /* stack: 1.name,2.binding last found,3.environment   */
         exp_reg =pop_pointer();
         unev_reg=pop_pointer();
         env_reg =pop_pointer();
         /* check if anything changed */
         if (unev_reg!=binding_in_frame(exp_reg,first_frame(env_reg))) {
            printf("RUNTIME-ERROR: binding for \"define\" changed during evaluation of ");
            write_call(exp_reg);
            cont_reg=ERROR_LABEL;
            break;
         }
         if (unev_reg==NIL) {
            define_variable_w(exp_reg,val_reg,env_reg);
         }
         else {
            set_variable_w(exp_reg,val_reg,env_reg);
         }
         val_reg=NIL;
         cont_reg=pop_label();
         break;
         /*}}}  */
      
      case AND_CONT_LABEL:
      
         /*{{{  loop for "and" evaluation --*/
         /* registers: val contains result of first argument */
         /* stack: 1.rest of arguments,2.environment */
         exp_reg=pop_pointer();
         env_reg=pop_pointer();
         if (val_reg==false_zap) {
             /* return at once */
             cont_reg=pop_label();
         }
         else {
            if (cdr(exp_reg)!=NIL) {
               /* go on looping */
               push_label(AND_CONT_LABEL);
               push_pointer(env_reg);
               push_pointer(cdr(exp_reg));
            }
            exp_reg=car(exp_reg);
            cont_reg=START_LABEL;
         }
         break;
         /*}}}  */
      
      case OR_CONT_LABEL:
      
         /*{{{  loop for "or" evaluation --*/
         /* registers: val contains result of first evaluation */
         /* stack: 1.rest of arguments, 2.environment */
         exp_reg=pop_pointer();
         env_reg=pop_pointer();
         if (val_reg!=false_zap) {
            /* immediate return */
            cont_reg=pop_label();
         }
         else {
            if (cdr(exp_reg)!=NIL) {
               /* go on looping */
               push_label(OR_CONT_LABEL);
               push_pointer(env_reg);
               push_pointer(cdr(exp_reg));
            }
            exp_reg=car(exp_reg);
            cont_reg=START_LABEL;
         }
         break;
         /*}}}  */
      
      case ASSIGNMENT_CONT_LABEL:
      
         /*{{{  second part for "set!" --*/
         /* registers: val contains the evaluated body */
         /* stack: 1.name for variable,2.binding found,3.environment */
         exp_reg =pop_pointer();
         unev_reg=pop_pointer();
         env_reg =pop_pointer();
         /* check if anything changed */
         if (unev_reg!=binding_in_env(exp_reg,env_reg)) {
            printf("RUNTIME-ERROR: binding for \"set!\" changed during evaluation of ");
            write_call(exp_reg);
            cont_reg=ERROR_LABEL;
            break;
         }
         set_variable_w(exp_reg,val_reg,env_reg);
         val_reg=NIL;
         cont_reg=pop_label();
         break;
         /*}}}  */
      
      case CONDITIONAL_CONT_LABEL:
      
         /*{{{  second part for "conditional" --*/
         /* registers: val contains the first evaluated condition */
         /* stack: 1.consequent-list,2.rest of clauses,3.environment */
         /*        4. original else-clause */
         exp_reg=pop_pointer();
         unev_reg=pop_pointer();
         env_reg=pop_pointer();
         if (val_reg!=false_zap) {
            /* hit gold, remove original clause */
            pop_pointer();
            if (exp_reg!=NIL) {
               /* evaluate consequents */
               cont_reg=EVAL_SEQUENCE_LABEL;
            }
            else cont_reg=pop_label();
         }
         else if (unev_reg==NIL) {
            /* no more clauses */
            printf("RUNTIME-ERROR: conditional w/o else-clause in ");
            write_call(pop_pointer());
            cont_reg=ERROR_LABEL;
         }
         else if (car(car(unev_reg))==else_zap) {
            /* this is the end */
            pop_pointer();
            exp_reg=cdr(car(unev_reg));
            cont_reg=EVAL_SEQUENCE_LABEL;
         }
         else {
            /* loop till something gives */
            push_label(CONDITIONAL_CONT_LABEL);
            push_pointer(env_reg);
            push_pointer(cdr(unev_reg));
            exp_reg=car(unev_reg);
            push_pointer(cdr(exp_reg));
            exp_reg=car(exp_reg);
            cont_reg=START_LABEL;
         }
         break;
         /*}}}  */
      
      /*}}}  */

      /*{{{  evaluation of a chain --*/
      
      case EVAL_SEQUENCE_LABEL:
      
         /*{{{  start evaluation of chain --*/
         /* registers: exp contains expressions, env contains environment */
         assert(exp_reg!=NIL);
         if (cdr(exp_reg)!=NIL) {
            /* more than one evaluation */
            push_label(EVAL_SEQUENCE_CONT_LABEL);
            push_pointer(env_reg);
            push_pointer(cdr(exp_reg));
         }
         exp_reg=car(exp_reg);
         cont_reg=START_LABEL;
         break;
         /*}}}  */
      
      case EVAL_SEQUENCE_CONT_LABEL:
      
         /*{{{  continue evaluation of chain, result in val --*/
         /* registers: val contains result of last evaluation */
         /* stack: 1.rest of chain, 2.environment */
         exp_reg=pop_pointer();
         env_reg=pop_pointer();
         if (cdr(exp_reg)!=NIL) {
            push_label(EVAL_SEQUENCE_CONT_LABEL);
            push_pointer(env_reg);
            push_pointer(cdr(exp_reg));
         }
         exp_reg=car(exp_reg);
         cont_reg=START_LABEL;
         break;
         /*}}}  */
      
      /*}}}  */

      case ERROR_LABEL:

         goto_recoverable_error();
         break;

      default:

         printf("PROGRAM ERROR: unknown label %i.\n",cont_reg);
         goto_recoverable_error();

      }
   } while (cont_reg!=END_LABEL);
}
/*}}}  */

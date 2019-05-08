#ifndef HELP_H
#define HELP_H

#include "memory.h"

extern bool     even_p(ulong val);
extern char     printit(char ch);

/* environment and frame manipulation */

extern ipointer first_frame(ipointer cur);
extern ipointer parent(ipointer cur);
extern ipointer create_begin_env(void);
extern ipointer binding_in_frame(ipointer var,ipointer frame);
extern ipointer binding_in_env(ipointer var,ipointer env);
extern ipointer first_binding(ipointer cur);
extern ipointer rest_bindings(ipointer cur);

extern void     define_variable_w(ipointer var,ipointer val,ipointer env);
extern void     set_variable_w(ipointer var,ipointer val,ipointer env);
extern ipointer extend_environment(ipointer vars,ipointer vals,ipointer base_env);

/* procedure manipulation */

extern ipointer proc_env(ipointer cur);
extern ipointer proc_text(ipointer cur);
extern ipointer proc_body(ipointer cur);
extern ipointer proc_params(ipointer cur);

/* syntax checks */

extern bool     symbol_list_p(ipointer cur);
extern bool     symbol_compound_p(ipointer cur);
extern bool     list_of_clauses_p(ipointer cur);
extern bool     assoc_list_p(ipointer cur);
extern bool     list_p(ipointer cur);
extern bool     unique_vars_p(ipointer vars);

/* syntax transformations */

extern ipointer separate_assoc(ipointer list);
extern ipointer clauses(ipointer expr);

/* expression manipulation */

extern ipointer operator(ipointer cur);
extern ipointer operands(ipointer cur);
extern ipointer first_arg(ipointer cur);
extern ipointer second_arg(ipointer cur);
extern ipointer third_arg(ipointer cur);
extern int      length(ipointer cur);
extern ipointer cons(ipointer a,ipointer b);

/* bindings */

extern ipointer adjoin_binding(ipointer binding,ipointer frame);
extern ipointer binding_variable(ipointer cur);
extern ipointer binding_value(ipointer cur);

#endif

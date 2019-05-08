#ifndef MEMORY_H
#define MEMORY_H

#define NIL NULL

typedef enum {TRUE=1,FALSE=0}    bool;
typedef unsigned long int        ulong;
typedef unsigned char            uchar;
typedef unsigned int             uint;
typedef unsigned long int huge  *ipointer;
typedef unsigned char huge      *cpointer;

/* memory configuration -- */

extern const ulong CBSLD;
extern const ulong DSLD;
extern const ulong STACKD;
extern const ulong REVSTACKD;
extern const ulong LSTACKD;

/* scheme machine registers -- */

extern ipointer  val_reg;
extern ipointer  env_reg;
extern ipointer  fun_reg;
extern ipointer  argl_reg;
extern ipointer  exp_reg;
extern ipointer  unev_reg;
extern uchar     cont_reg;

/* Transforming a pointer to a "zap-special value" */

extern  ipointer set_zap_special(ipointer cur);

/* Setting and querying a storage typedescriptor */

extern  uint     get_typedesc(ipointer cur);
extern  void     set_typedesc(ipointer cur,uint td);

/* Pushing and popping labels & pointers */

extern  uchar    pop_label(void);
extern  void     push_label(uchar label);
extern  ipointer pop_pointer(void);
extern  void     push_pointer(ipointer ptr);
extern  void     revpush_pointer(ipointer cur);

/* Getting and setting the car and cdr of a cons-box */
/* Note that special bits are filtered, except for "zap-special" */

extern  ipointer car(ipointer cur);
extern  ipointer cdr(ipointer cur);
extern  void     set_car(ipointer this,ipointer that);
extern  void     set_cdr(ipointer this,ipointer that);

/* Setting and querying hints of a cons-box: "Procedure" or "Env. header" */

extern  void     set_hint_procedure(ipointer this);
extern  void     set_hint_environment(ipointer this);
extern  bool     hint_environment_p(ipointer this);
extern  bool     hint_procedure_p(ipointer this);

/* Allocation of storage place; "size" is the size in bytes! */

extern  ipointer new_cons(void);
extern  ipointer new_storage(ulong size);

/* Queries: special bits of pointer set, pointer to a cbox or to storage? */

extern  bool     special_p(ipointer cur);
extern  bool     cbox_p(ipointer cur);
extern  bool     storage_p(ipointer cur);

/* Garbage collection, initialization and statistics */

extern  void     garbage_collect(void);
extern  void     init_mem(void);
extern  void     init_registers(void);
extern  void     init_stack(void);
extern  void     dump_state(void);
extern  void     statistics_mem(void);   /* To stdout */
extern  ulong    stat_cbox_free(void);
extern  ulong    stat_storage_free(void);
extern  ulong    stat_storage_blocs(void);
extern  ulong    stat_stack_free(void);
extern  ulong    stat_lstack_free(void);
extern  void     cleanup_mem(void);

#endif

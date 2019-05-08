/* ===========================================================================
   Parser module
   -------------
   This is a simple recursive descent, backtracking parser. If an error occurs
   during parsing, the error is printed out and the input read up to a double
   newline ("\n\n") to synchronize again.

   Return value is a pointer to a created structure or NIL, as well as a
   status bit. This bit may be:

   OK   : Everything worked fine.
   STOP : Everything worked fine but an EOF was encountered. However, the
          pointer still points to interesting data.
   ERROR: An error occurred, but the input has been resynchronized.
   TERM : Either an EOF was prematurely encountered, or only an EOF was
          read. In both cases, there is nothing to evaluate and the program
          should shutdown.
   BACK : Backtrack; possibly another category might lead to success. This
          value is never returned at top level.

   Ringbuffer
   ----------
   To implement read-ahead, a ringbuffer has been implemented. See the code
   for details.

   Conventions
   -----------
   "parse_datum()" is the main dispatch routine. It calls the various
   parsing procedures one after another. Before calling them,
   "start_read_ahead()" is called, so that a buffer overflow during read-ahead
   can be detected. "get_firstchar()" will return ERROR if this occurs. As
   soon as a parsing procedure is sure that the category matches, it must
   call "confirm_accept()" to switch off buffer overflow checking, and may
   then read the stream at will, using "set_stopmark()" or "reset_readmark()"
   to store and reset the reading position. If the category doesn't match,
   the parsing procedure has to return BACK, "parse_datum()" will reset the
   reading mark.
   Every parsing procedure may be certain that the first character it will
   read is valid and that it is not a whitespace. If the parsing is complete,
   any unused character has to be put back into the buffer; for this purpose,
   "back_char()" may be used.
   Dont't call "remove_whitespace()" before you have called "confirm_accept()",
   the buffer might easily overflow if there is a somewhat lengthy comment.

   Syntax structure of input data:
   -------------------------------
   <parse-element> ::= <datum>.
           <datum> ::= <quoted_datum> | <char_datum> | <p-expr> | <string> |
                       <boolean> | <integer> | <hexint> | <float> | <symbol>.
    <quoted_datum> ::= '<datum>.
      <char_datum> ::= "#\space" | "#\newline" | "#\"<character>.
          <p-expr> ::= "()" | "(" <datum> {<datum>} [" . " <datum>] ")".
          <string> ::= """{ { <character> } ["\""|"\\"] }""".
         <boolean> ::= "#t" | "#T" | "#f" | "#F".
         <integer> ::= ["#d"|"#D"]["+"|"-"]<digit>{<digit>}.
          <hexint> ::= ["#x"|"#X"]["+"|"-"]<hexdigit>{<hexdigit>}.
           <float> ::= ["+"|"-"]({<digit>}"."<digit>{<digit>}[<exponent>] |
                       <digit>{<digit>}<exp>).
        <exponent> ::= ["E"|"e"]["+"|"-"]<digit>{<digit>}.
          <symbol> ::= (<alpha>|<digit>|<special>)
                       {<alpha>|<digit>|<special>|<point>}.

  <alpha>    ::= "a"|"b"|"c"|"d"|"e"|"f"|"g"|"h"|"i"|"j"|"k"|"l"|"m"|"n"|"o"|
                 "p"|"q"|"r"|"s"|"t"|"u"|"v"|"w"|"x"|"y"|"z"|"A"|"B"|"C"|"D"|
                 "E"|"F"|"G"|"H"|"I"|"J"|"K"|"L"|"M"|"N"|"O"|"P"|"Q"|"R"|"S"|
                 "T"|"U"|"V"|"W"|"X"|"Y"|"Z".
  <digit>    ::= "1"|"2"|"3"|"4"|"5"|"6"|"7"|"8"|"9"|"0".
  <special>  ::= "*"|"/"|"<"|"="|">"|"!"|"?"|":"|"$"|"%"|"_"|"&"|"^"|"~"|"-"|
                 "+".
  <point>    ::= ".".
  <hexdigit> ::= <digit>|"a"|"b"|"c"|"d"|"e"|"f"|"A"|"B"|"C"|"D"|"E"|"F".

=========================================================================== */

/*{{{  includes --*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "memory.h"
#include "parser.h"
#include "magic.h"
#include "help.h"
/*}}}  */

#define DEBUGPARSER      /* Debugging on */
#undef  DEBUGPARSER

/*{{{  defines --*/
#define SYMLEN      40   /* Maximal length of a symbol */
#define SCREENWIDTH 80   /* Assumed size of console    */
#define IDENTLEN    10   /* Maximal length of character identifier */
#define STRLEN      256  /* Maximal length of string */
/*}}}  */

/*{{{  structure of ringbuffer --*/
typedef struct RINGBUF {
           char buf[RINGSIZE];
           bool eof;              /* eof reached                         */
           FILE *stream;          /* file from which to read             */
           int  readmark;         /* ring position to read from          */
           int  writemark;        /* ring position to write to           */
           int  stopmark;         /* to remember some position           */
           int  backmark;         /* start of read-ahead                 */
     } ringbuffer_desc;

        /* "stopmark" is for temporarily remembering a position; it is    */
        /* affected by "set_stopmark()" and "reset_readmark()". Before    */
        /* starting read-ahead, "backmark" must be set so that one may    */
        /* later set back the readmark. If backmark<0, it is unset. A     */
        /* check is made if reading a character will overwrite the        */
        /* backmark position.                                             */
/*}}}  */

/*{{{  procedure headers --*/
static char firstchar(ringbuffer rb,status *res);
static void back_char(ringbuffer rb);
static void set_stopmark(ringbuffer rb);
static void reset_readmark(ringbuffer rb);
static void confirm_accept(ringbuffer rb);
static void start_read_ahead(ringbuffer rb);
static void back_read_ahead(ringbuffer rb);
static void clean_buffer(ringbuffer rb);
static void dump_buffer(ringbuffer rb);
static char datain(bool *noteof,FILE *stream);
static bool terminal_p(char ch);
static bool alpha_p(char ch);
static bool whitespace_p(char ch);
static bool digit_p(char ch);
static bool specialchar_p(char ch);
static int value(char ch);
static void remove_whitespace(ringbuffer rb,status *res);
static void synchronize(ringbuffer rb,status *res);
static ipointer parse_quoted(ringbuffer rb,status *res);
static ipointer parse_character(ringbuffer rb,status *res);
static ipointer parse_list(ringbuffer rb,status *res);
static ipointer parse_string(ringbuffer rb,status *res);
static ipointer parse_boolean(ringbuffer rb,status *res);
static ipointer parse_integer(ringbuffer rb,status *res);
static ipointer parse_symbol(ringbuffer rb,status *res);
static ipointer parse_datum(ringbuffer rb,status *res);
/*}}}  */

/* ======================================================================== */
/* Ringbuffer management                                                    */
/* ======================================================================== */

/*{{{  allocation of a new ringbuffer; returns NULL on error --*/
ringbuffer new_ringbuffer(FILE *stream) {
   ringbuffer rb;
   rb=(ringbuffer)malloc(sizeof(ringbuffer_desc));
   if (rb==NULL) return NULL;
   else {
      clean_buffer(rb);
      rb->stream=stream;
      return rb;
   }
}
/*}}}  */

/*{{{  freeing an old ringbuffer*/
void release_ringbuffer(ringbuffer rb) {
   if (fclose(rb->stream)==EOF) {
      printf("Error occured while closing stream.\n");
   }
   free((void *)rb);
}
/*}}}  */

/*{{{  getting a char from a ringbuffer; returns OK,STOP,ERROR --*/
static char firstchar(ringbuffer rb,status *res) {
   char ch;
   bool neof;
   if (rb->readmark==rb->writemark) {
      /* a new character must be read in */
      if (rb->eof) {
         *res=STOP;ch='\0';
      }
      else if (rb->backmark>=0 && rb->writemark==rb->backmark) {
         /* overtook backmark during read-ahead */
         *res=ERROR;ch='\0';
      }
      else {
      (rb->buf)[rb->writemark]=datain((bool *)&neof,rb->stream);
      rb->writemark=(rb->writemark+1)%RINGSIZE;
      ch=rb->buf[rb->readmark];
      rb->readmark=(rb->readmark+1)%RINGSIZE;
      if (!neof) { *res=STOP;rb->eof=TRUE; }
      else *res=OK;
      }
   }
   else {
      *res=OK;
      ch=rb->buf[rb->readmark];
      rb->readmark=(rb->readmark+1)%RINGSIZE;
   }
   return ch;
}

/* "res" is set to OK if everything went well, but to STOP if an eof */
/* occurred, and to ERROR if an overflow occurred during read-ahead. */
/*}}}  */

/*{{{  setting the readmark one char back --*/
static void back_char(ringbuffer rb) {
   /* One may not move back over the backmark */
   if (!(rb->backmark>=0 && rb->readmark==rb->backmark)) {
      rb->readmark=(rb->readmark+RINGSIZE-1)%RINGSIZE;
   }
}
/*}}}  */

/*{{{  set the stopmark to the current reading position --*/
static void set_stopmark(ringbuffer rb) {
   rb->stopmark=rb->readmark;
}
/*}}}  */

/*{{{  reset the readmark to the stopmark --*/
static void reset_readmark(ringbuffer rb) {
   rb->readmark=rb->stopmark;
}
/*}}}  */

/*{{{  unset the backmark to stop read-ahead --*/
static void confirm_accept(ringbuffer rb) {
   assert(rb->backmark>=0);
   rb->backmark=-1;
}
/*}}}  */

/*{{{  set the backmark to begin read_ahead --*/
static void start_read_ahead(ringbuffer rb) {
   assert(rb->backmark<0);
   rb->backmark=rb->readmark;
}
/*}}}  */

/*{{{  reset the readmark to start of read-ahead, clearing the backmark --*/
static void back_read_ahead(ringbuffer rb) {
   assert(rb->backmark>=0);
   rb->readmark=rb->backmark;
   rb->backmark=-1;
}
/*}}}  */

/*{{{  the buffer is initialized --*/
static void clean_buffer(ringbuffer rb) {
   int i;
   for (i=0;i<RINGSIZE;i++) rb->buf[i]='\0';
   rb->writemark=0;
   rb->stopmark=0;
   rb->backmark=-1;
   rb->readmark=0;
   rb->eof=FALSE;
}
/*}}}  */

/*{{{  buffer contents are written to stdout --*/
static void dump_buffer(ringbuffer rb) {
   int i,j;
   i=rb->readmark;
   j=0;
   do {
      printf("%c",printit(rb->buf[i]));
      i=(i+1)%RINGSIZE;
      j++;
      if (j==SCREENWIDTH) {
         j=0;
         printf("\n");
      }
   } while (i!=rb->readmark);
   if (j!=0) printf("\n");
}
/*}}}  */

/* ======================================================================== */
/* Auxiliary procedures                                                     */
/* ======================================================================== */

/*{{{  reading data from stream --*/
static char datain(bool *noteof,FILE *stream) {
   char ch;
   fread(&ch,sizeof(char),1,stream);
   /* set status if reading ok */
   *noteof=!feof(stream);
   if (*noteof) return ch;
   else return '\0';
}
/*}}}  */

/*{{{  terminating character? --*/
static bool terminal_p(char ch) {
   return (whitespace_p(ch) || ch=='(' || ch==')' || ch==';');
}
/*}}}  */

/*{{{  alphabetic character? --*/
static bool alpha_p(char ch) {
   return ((0x41<=ch && ch<=0x5A) || (0x61<=ch && ch<=0x7A));
}
/*}}}  */

/*{{{  whitespace character? --*/
static bool whitespace_p(char ch) {
   return (ch==' ' || ch=='\t' || ch=='\n');
}
/*}}}  */

/*{{{  digit character? --*/
static bool digit_p(char ch) {
   return (0x30<=ch && ch<=0x39);
}
/*}}}  */

/*{{{  special character? --*/
static bool specialchar_p(char ch) {
   return (ch=='*' || ch=='/' || ch=='<' || ch=='=' || ch=='>' ||
           ch=='!' || ch=='?' || ch==':' || ch=='$' || ch=='%' ||
           ch=='_' || ch=='&' || ch=='^' || ch=='~' || ch=='-' ||
           ch=='+' || ch=='.');
}
/*}}}  */

/*{{{  numerical value of hexdigit --*/
static int value(char ch) {
   if (digit_p(ch))
      return ch-'0';
   else if (ch=='a' || ch=='b' || ch=='c' || ch=='d' || ch=='f')
      return ch-'a'+10;
   else
      return ch-'A'+10;
}
/*}}}  */

/*{{{  remove whitespaces --*/
/* May return STOP if an EOF occurred, or OK if all is well. */
/* NEVER called during read-ahead.                           */
static void remove_whitespace(ringbuffer rb,status *res) {
   char ch;
   #ifdef DEBUGPARSER
   printf("parser.c: remove_whitespace() called.\n");
   #endif
   assert(rb->backmark<0);
   do {
      do {
         ch=firstchar(rb,res);
      } while (*res==OK && whitespace_p(ch));
      if (*res==OK && ch==';') {
         do {
            ch=firstchar(rb,res);
         } while (*res==OK && ch!='\n');
      }
  } while (*res==OK && whitespace_p(ch));
  assert(*res==OK || *res==STOP);
  if (*res==OK) back_char(rb);
}
/*}}}  */

/*{{{  resynchronize input; returns STOP or OK --*/
/* Flushes input up to the first "\n\n". */
/* NEVER called during read-ahead.       */
static void synchronize(ringbuffer rb,status *res) {
   char ch;
   #ifdef DEBUGPARSER
   printf("parser.c: synchronize() called.\n");
   #endif
   assert(rb->backmark<0);
   /* Input is flushed up to "\n\n" */
   *res=ERROR;
   printf("syn:");
   while (*res==ERROR) {
      do {
         ch=firstchar(rb,res);
      } while (*res!=STOP && ch!='\n');
      if (*res!=STOP) {
         printf("\nsyn:");
         ch=firstchar(rb,res);
      }
      if (*res!=STOP && ch=='\n') {
         *res=OK;
         printf("\n");
      }
      else if (*res!=STOP) *res=ERROR;
   }
}
/*}}}  */

/* ======================================================================== */
/* Parsing procedures                                                       */
/* ======================================================================== */

/*{{{  parsing of a quotation; returns OK-STOP-TERM-ERROR-BACK --*/
static ipointer parse_quoted(ringbuffer rb,status *res) {
   char     ch;
   ipointer ip;
   #ifdef DEBUGPARSER
   printf("parser.c: parse_quoted() called.\n");
   #endif
   ch=firstchar(rb,res);
   assert(*res==OK);
   if (ch=='\'') {
      confirm_accept(rb);
      remove_whitespace(rb,res);
      assert(*res==OK || *res==STOP);
      if (*res==STOP) {
         printf("PARSE-ERROR: early EOF reading quoted expression.\n");
         *res=TERM;return NIL;
      }
      ip=parse_datum(rb,res);
      assert(*res==OK || *res==STOP || *res==TERM || *res==ERROR);
      if (*res==OK || *res==STOP) {
         push_pointer(ip);
         ip=new_cons();set_car(ip,pop_pointer());
         push_pointer(ip);
         ip=new_cons();
         set_car(ip,make_symbol("quote"));
         set_cdr(ip,pop_pointer());
         return ip;
      }
      else {
         return NIL;
      }
   }
   else {
      *res=BACK;return NIL;
   }
}
/*}}}  */

/*{{{  parsing of a character; returns OK-STOP-TERM-ERROR-BACK --*/
static ipointer parse_character(ringbuffer rb,status *res) {
   char ch,ident[IDENTLEN+1];
   int  i;
   #ifdef DEBUGPARSER
   printf("parser.c: parse_character() called.\n");
   #endif
   ch=firstchar(rb,res);
   assert(*res==OK);
   if (ch=='#') {
      ch=firstchar(rb,res);
      assert(*res==STOP || *res==OK);
      if (*res==STOP) {
         printf("PARSE-ERROR: early EOF reading hash-expression.\n");
         *res=TERM;return NIL;
      }
      if (ch=='\\') {
         confirm_accept(rb);
         ch=firstchar(rb,res);
         assert(*res==STOP || *res==OK);
         if (*res==STOP) {
            printf("PARSE-ERROR: early EOF reading character-expression.\n");
            *res=TERM;return NIL;
         }
         ident[0]=ch;
         ch=firstchar(rb,res);
         assert(*res==STOP || *res==OK);
         if (*res==STOP || terminal_p(ch)) {
            if (*res==OK) back_char(rb);
            return make_char((int)((uchar)ident[0]));
         }
         i=1;
         while (*res==OK && alpha_p(ch) && i<IDENTLEN) {
            ident[i]=ch;i++;ch=firstchar(rb,res);
            assert(*res==STOP || *res==OK);
         }
         ident[i]='\0';
         if (*res==OK && alpha_p(ch)) {
            printf("PARSE-ERROR: char-ident \"%s...\" too long.\n",ident);
            *res=ERROR;return NIL;
         }
         else if (*res==OK && !terminal_p(ch)) {
            printf("PARSE-ERROR: illegal char %c in ident \"%s\".\n",
                    printit(ch),ident);
            *res=ERROR;return NIL;
         }
         else {
            back_char(rb);
            if (strcmp(ident,"newline")==0) {
               return make_char((int)((uchar)('\n')));
            }
         else if (strcmp(ident,"space")==0) {
               return make_char((int)((uchar)(' ')));
            }
            else {
               printf("PARSE-ERROR: unknown char-ident \"%s\".\n",ident);
               if (*res==STOP) *res=TERM; else *res=ERROR;
               return NIL;
            }
         }
      }
   }
   *res=BACK;return NIL;
}
/*}}}  */

/*{{{  parsing of parenthesized expr; returns OK-STOP-TERM-ERROR-BACK --*/
static ipointer parse_list(ringbuffer rb,status *res) {
   char     ch;
   ipointer ip,ipold,ipnew,ipdown;
   bool     pointcdr;
   #ifdef DEBUGPARSER
   printf("parser.c: parse_list() called.\n");
   #endif
   ch=firstchar(rb,res);
   assert(*res==OK);
   ipold=NIL; /* to shut up warning */
   if (ch=='(') {
      confirm_accept(rb);
      remove_whitespace(rb,res);
      assert(*res==STOP || *res==OK);
      set_stopmark(rb);
      ch=firstchar(rb,res);
      assert(*res==STOP || *res==OK);
      if (*res==STOP) {
         printf("PARSE-ERROR: early EOF reading parenthesized expression.\n");
         *res=TERM;return NIL;
      }
      ip=NIL;
      while (ch!=')') {
         pointcdr=FALSE;
         if (ch=='.') {
            ch=firstchar(rb,res);
            assert(*res==STOP || *res==OK);
            if (*res==OK && whitespace_p(ch)) {
               pointcdr=TRUE;
               remove_whitespace(rb,res);
               assert(*res==STOP || *res==OK);
               set_stopmark(rb);
            }
         }
         if (*res==STOP) {
            printf("Parse-error: early EOF reading parenthesized expression.\n");
            *res=TERM;return NIL;
         }
         reset_readmark(rb);
         push_pointer(ip);ipdown=parse_datum(rb,res);pop_pointer();
         assert(*res==OK || *res==STOP || *res==TERM || *res==ERROR);
         if (*res==STOP) {
            printf("Parse-error: Early EOF reading parenthesized expression!\n");
            *res=TERM;return NIL;
         }
         else if (*res==ERROR || *res==TERM) return NIL;
         if (pointcdr && ip==NIL) {
            printf("PARSE-ERROR: cons-box without car.\n");
            *res=ERROR;return NIL;
         }
         else if (pointcdr) {
            set_cdr(ipold,ipdown);
         }
         else if (ip==NIL) {
            push_pointer(ipdown);ip=new_cons();
            set_car(ip,pop_pointer());set_cdr(ip,NIL);ipold=ip;
         }
         else {
            push_pointer(ip);push_pointer(ipdown);
            ipnew=new_cons();
            pop_pointer();pop_pointer();
            set_car(ipnew,ipdown);set_cdr(ipnew,NIL);set_cdr(ipold,ipnew);
            ipold=ipnew;
         }
         remove_whitespace(rb,res);
         assert(*res==STOP || *res==OK);
         set_stopmark(rb);
         ch=firstchar(rb,res);
         assert(*res==STOP || *res==OK);
         if (*res==STOP) {
            printf("PARSE-ERROR: early EOF reading parenthesized expression.\n");
            *res=TERM;return NIL;
         }
         if (pointcdr && ch!=')') {
            printf("PARSE-ERROR: Illegal \"%c\" instead of final \")\".\n",
                    printit(ch));
            *res=ERROR;return NIL;
         }
      }
      return ip;
   }
   else {
      *res=BACK;return NIL;
   }
}
/*}}}  */

/*{{{  parsing of a string; returns OK-STOP-TERM-ERROR-BACK --*/
static ipointer parse_string(ringbuffer rb,status *res) {
   char     ch,string[STRLEN+1];
   int      i;
   #ifdef DEBUGPARSER
   printf("parser.c: parse_string() called.\n");
   #endif
   ch=firstchar(rb,res);
   assert(*res==OK);
   if (ch=='\"') {
      confirm_accept(rb);
      ch=firstchar(rb,res);
      assert(*res==OK || *res==STOP);
      i=0;
      while (*res==OK && ch!='\"' && i<STRLEN) {
         while (*res==OK && ch!='\"' && ch!='\\' && ch!='\n' && i<STRLEN) {
            string[i]=ch;i++;ch=firstchar(rb,res);
            assert(*res==OK || *res==STOP);
         }
         if (*res==OK && ch=='\\' && i<STRLEN) {
            ch=firstchar(rb,res);
            assert(*res==OK || *res==STOP);
            if (*res==OK) {
               if (ch=='n') string[i]='\n';
               else         string[i]=ch;
               i++;ch=firstchar(rb,res);
               assert(*res==OK || *res==STOP);
            }
         }
         else if (*res==OK && ch=='\n') {
            ch=firstchar(rb,res);
            assert(*res==OK || *res==STOP);
         }
      }
      string[i]='\0';
      if (*res==OK && ch!='\"') {
         string[10]='\0';
         printf("PARSE-ERROR: string beg. with \"%s...\" too long.\n",string);
         *res=ERROR;return NIL;
      }
      else if (*res==OK) {
         return make_string(string);
      }
      else {
         printf("PARSE-ERROR: unexpected EOF in string \"%s...\".\n",string);
         *res=TERM;return NIL;
      }
   }
   else {
      *res=BACK;return NIL;
   }
}
/*}}}  */

/*{{{  parsing of a boolean; returns OK-STOP-TERM-*-BACK --*/
static ipointer parse_boolean(ringbuffer rb,status *res) {
   char ch,chx;
   #ifdef DEBUGPARSER
   printf("parser.c: parse_boolean() called.\n");
   #endif
   ch=firstchar(rb,res);
   assert(*res==OK);
   if (ch=='#') {
      ch=firstchar(rb,res);
      assert(*res==OK || *res==STOP);
      if (*res==STOP) {
         printf("PARSE-ERROR: early EOF reading hash-expression.\n");
         *res=TERM;return NIL;
      }
      if (ch=='t' || ch=='T' || ch=='f' || ch=='F') {
         chx=firstchar(rb,res);
         assert(*res==OK || *res==STOP);
         if (*res==STOP || terminal_p(chx)) {
            confirm_accept(rb);
            if (*res==OK) back_char(rb);
            return make_bool(ch=='t' || ch=='T');
         }
      }
   }
   *res=BACK;return NIL;
}
/*}}}  */

/*{{{  parsing of an integer; returns OK-STOP-TERM-ERROR-BACK --*/
static ipointer parse_integer(ringbuffer rb,status *res) {
   char     ch;
   long int val;
   int      sign=1;
   bool     isinteger=FALSE;
   #ifdef DEBUGPARSER
   printf("parser.c: parse_integer() called.\n");
   #endif
   ch=firstchar(rb,res);
   assert(*res==OK);
   if (ch=='#') {
      ch=firstchar(rb,res);
      assert(*res==STOP || *res==OK);
      if (*res==STOP) {
         printf("PARSE-ERROR: early EOF reading hash-expression.\n");
         *res=TERM;return NIL;
      }
      if (ch=='d' || ch=='D') {
         confirm_accept(rb);
         isinteger=TRUE;
         ch=firstchar(rb,res);
         assert(*res==STOP || *res==OK);
         if (*res==STOP) {
            printf("PARSE-ERROR: early EOF reading integer.\n");
            *res=TERM;return NIL;
         }
      }
      else {
         *res=BACK;return NIL;
      }
   }
   if (ch=='-' || ch=='+') {
      if (ch=='-') sign=-1;
      ch=firstchar(rb,res);
      assert(*res==STOP || *res==OK);
      if (*res==STOP && isinteger) {
         printf("PARSE-ERROR: early EOF reading integer.\n");
         *res=TERM;return NIL;
      }
   }
   if (digit_p(ch)) {
      val=0;
      while (*res==OK && ch=='0') ch=firstchar(rb,res);
      if (*res==ERROR) {
         printf("PARSE-ERROR: read ahead too far during 0-string.\n");
         return NIL;
      }
      if (*res==STOP) {
         if (!isinteger) confirm_accept(rb);
         return make_int(0);
      }
      while (*res==OK && digit_p(ch)) {
         if (!((sign==-1 && val>=(LONG_MIN+value(ch))/10) ||
               (sign==1  && val<=(LONG_MAX-value(ch))/10)))  {
            printf("PARSE-ERROR: integer too large.\n");
            *res=ERROR;return NIL;
         }
         val=val*10+sign*value(ch);
         ch=firstchar(rb,res);
      }
      if (*res==ERROR) {
         printf("PARSE-ERROR: read ahead too far while parsing integer.\n");
         return NIL;
      }
      if (*res==STOP) {
         if (!isinteger) confirm_accept(rb);
         return make_int(val);
      }
      else if (*res==OK && terminal_p(ch)) {
         if (!isinteger) confirm_accept(rb);
         back_char(rb);return make_int(val);
      }
      else if (isinteger) {
         printf("PARSE-ERROR: integer contains illegal \"%c\".\n",printit(ch));
         *res=ERROR;return NIL;
      }
      else {
         *res=BACK;
         return NIL;
      }
   }
   else if (isinteger) {
      printf("PARSE-ERROR: integer contains illegal \"%c\".\n",printit(ch));
      *res=ERROR;return NIL;
   }
   else {
      *res=BACK;return NIL;
   }
}
/*}}}  */

/*{{{  parsing of a symbol; returns OK-STOP-*-ERROR-BACK --*/
static ipointer parse_symbol(ringbuffer rb,status *res) {
   char ch,symbol[SYMLEN+1];
   int  i=0;
   #ifdef DEBUGPARSER
   printf("parser.c: parse_symbol() called.\n");
   #endif
   ch=firstchar(rb,res);
   assert(*res==OK);
   while ((digit_p(ch)||alpha_p(ch)||specialchar_p(ch)) && i<SYMLEN && *res==OK) {
       symbol[i]=ch;i++;
       ch=firstchar(rb,res);
       assert(*res==OK || *res==STOP); /* i flows over before buffer */
   }
   symbol[i]='\0';
   if ((*res==OK && !terminal_p(ch) && !digit_p(ch) &&
      !alpha_p(ch) && !specialchar_p(ch)) || i==0) {
      /* something unexpected happened */
      *res=BACK;return NIL;
   }
   else if (*res==OK && (digit_p(ch) || alpha_p(ch) || specialchar_p(ch))) {
      symbol[10]='\0';
      printf("PARSE-ERROR: Symbol beg. with \"%s...\" too long.\n",symbol);
      *res=ERROR;return NIL;
   }
   else if (*res==OK && terminal_p(ch) && !(i==1 && symbol[0]=='.')) {
      confirm_accept(rb);
      back_char(rb);return make_symbol(symbol);
   }
   else if (*res==STOP && !(i==1 && symbol[0]=='.')) {
      confirm_accept(rb);
      return make_symbol(symbol);
   }
   else {
      *res=BACK;return NIL;
   }
}
/*}}}  */

/*{{{  parsing of a datum; returns OK-STOP-TERM-ERROR-* --*/
static ipointer parse_datum(ringbuffer rb,status *res) {
   ipointer ip;
   #ifdef DEBUGPARSER
   printf("parser.c: parse_datum() called.\n");
   #endif
   firstchar(rb,res);
   assert(*res==STOP || *res==OK);
   if (*res==STOP) {
      /* MUST read something, otherwise error */
      printf("PARSE-ERROR: early EOF reached.\n");
      *res=TERM;return NIL;
   }
   back_char(rb);start_read_ahead(rb);
   ip=parse_list(rb,res);
   if (*res==BACK) {
      back_read_ahead(rb);start_read_ahead(rb);ip=parse_boolean(rb,res);
   }
   else return ip;
   if (*res==BACK) {
      back_read_ahead(rb);start_read_ahead(rb);ip=parse_character(rb,res);
   }
   else return ip;
   if (*res==BACK) {
      back_read_ahead(rb);start_read_ahead(rb);ip=parse_quoted(rb,res);
   }
   else return ip;
   if (*res==BACK) {
      back_read_ahead(rb);start_read_ahead(rb);ip=parse_string(rb,res);
   }
   else return ip;
   if (*res==BACK) {
      back_read_ahead(rb);start_read_ahead(rb);ip=parse_integer(rb,res);
   }
   else return ip;
   if (*res==BACK) {
      back_read_ahead(rb);start_read_ahead(rb);ip=parse_symbol(rb,res);
   }
   else return ip;
   if (*res==BACK) {
      printf("PARSE-ERROR: unknown expression type.\n");
      *res=ERROR;return NIL;
   }
   else return ip;
}
/*}}}  */

/* ======================================================================== */

/*{{{  top-level read procedure; returns OK-STOP-TERM-ERROR-* --*/
ipointer read_call(ringbuffer rb,status *res) {
   ipointer ip;
   #ifdef DEBUGPARSER
   printf("parser.c: entering read_call.\n");
   #endif
   assert(rb->backmark<0);
   remove_whitespace(rb,res);
   assert(*res==STOP || *res==OK);
   if (*res==STOP) {
      printf("Empty input before EOF.\n");
      *res=TERM;ip=NIL;
   }
   else {
      ip=parse_datum(rb,res);
      if (*res==ERROR) {
         printf("Buffer content:\n");
         dump_buffer(rb);
         rb->backmark=-1;
         synchronize(rb,res);
         assert(*res==STOP || *res==OK);
         if (*res==STOP) {
            printf("EOF reached during synchronization.\n");
            *res=TERM;
         }
         else {
            *res=ERROR;
         }
         ip=NIL;
      }
      else {
         assert(*res==TERM || *res==STOP || *res==OK);
         assert((*res!=OK && *res!=STOP) || rb->backmark<0);
         if (*res==TERM) rb->backmark=-1;
      }
   }
   #ifdef DEBUGPARSER
   printf("parser.c: Buffer at exit of read_call():\n");
   dump_buffer(rb);
   #endif
   return ip;
}
/*}}}  */


%{
  let mk_seq operand =
    Seq.mk operand []

  let mk_binop l op r =
    let seq = Seq.seq_op_seq l op r in
    seq

  let mk_pat_parenthesized e =
    let e = UHPat.mk_OpSeq e in
    UHPat.Parenthesized(e)

  let mk_application e args =
    let e = mk_seq e in
    let rec mk_app e args =
      match args with
      | [] -> e
      | x::xs -> (
        let x = mk_seq x in
        let opseq = mk_app x xs in
        mk_binop e Operators_Exp.Space opseq
      )
    in
    mk_app e args

  let mk_let_line pat expr =
    let pat = UHPat.mk_OpSeq pat in
    UHExp.letline pat expr

  let mk_typ_paren typ =
    let opseq = UHTyp.mk_OpSeq typ in
    UHTyp.Parenthesized opseq

  let mk_typ_list typ =
    let opseq = UHTyp.mk_OpSeq typ in
    UHTyp.List opseq

  let mk_inj_l expr =
    UHExp.Inj(ErrStatus.NotInHole, InjSide.L, expr)

  let mk_inj_r expr =
    UHExp.Inj(ErrStatus.NotInHole, InjSide.R, expr)

  let mk_fn pat expr =
    let pat = UHPat.mk_OpSeq pat in
    UHExp.lam pat expr

  let mk_rule pat expr =
    let pat = UHPat.mk_OpSeq pat in
    UHExp.Rule(pat, expr)

  let mk_case expr rules =
    let e = UHExp.case expr rules in
    mk_seq e

  let mk_empty_list =
    mk_seq (UHExp.listnil ())
%}

%token LET
%token IN
%token <string> INT
%token <string> FLOAT
%token TRUE FALSE
%token PLUS MINUS
%token MULT DIV
%token FPLUS FMINUS
%token FMULT FDIV
%token COLON
%token COLONCOLON
%token SEMICOLON
%token EQUAL
%token GREATER LESSER
%token PERIOD
%token COMMA
%token INJL INJR
%token EOF
%token <string> IDENT
%token LPAREN RPAREN
%token LBRACE RBRACE
%token LBRACK RBRACK
%token LAMBDA
%token CASE
%token BAR
%token ARROW
%token TARROW
%token END
%token <string> COMMENT
%token EMPTY

%left LESSER GREATER EQUAL
%left PLUS MINUS FPLUS FMINUS
%left MULT DIV FMULT FDIV
%right COLONCOLON
%left BAR
%right TARROW
%left COMMA
%nonassoc LET LPAREN LAMBDA INT IDENT IN

%start main
%type <UHExp.t> main
%%

main:
  expr EOF { $1 }
  | expr SEMICOLON main EOF { List.concat [$1; $3] }
;

let_binding:
  LET pat EQUAL expr IN { mk_let_line $2 $4 }
  | LET pat COLON typ EQUAL expr IN { mk_let_line $2 $6 }
;

typ:
  typ typ_op typ { mk_binop $1 $2 $3 }
  | typ_ { mk_seq $1 }
;

typ_:
  atomic_type { $1 }
  | LPAREN typ RPAREN { mk_typ_paren $2 }
  | LBRACK typ RBRACK { mk_typ_list $2 }
;

atomic_type:
  IDENT {
    match $1 with
    | "Int" -> UHTyp.Int
    | "Bool" -> UHTyp.Bool
    | "Float" -> UHTyp.Float
    | _ -> failwith ("Unknown Type: "^$1)
  }
;

%inline typ_op:
  COMMA { Operators_Typ.Prod }
  | TARROW { Operators_Typ.Arrow }
  | BAR { Operators_Typ.Sum }
;

pat:
  pat COLONCOLON pat { mk_binop $1 Operators_Pat.Cons $3 }
  | pat COMMA pat { mk_binop $1 Operators_Pat.Comma $3 }
  | pat_ { mk_seq $1 }
;

pat_:
  LPAREN pat RPAREN { mk_pat_parenthesized $2 }
  | IDENT { UHPat.var $1 }
  | pat_constant { $1 }
  | LBRACK RBRACK { UHPat.listnil () }
;

pat_constant:
  INT { UHPat.intlit $1 }
  | FLOAT { UHPat.floatlit $1 }
  | TRUE { UHPat.boollit true }
  | FALSE { UHPat.boollit false }
;

expr:
  expr_ { [UHExp.ExpLine (UHExp.mk_OpSeq $1)] }
  | COMMENT expr { List.concat [[UHExp.CommentLine $1]; $2] }
  | EMPTY expr { List.concat [[UHExp.EmptyLine]; $2] }
  | let_binding expr { List.concat [[$1]; $2] }
;

expr_:
  simple_expr { mk_seq $1 }
  | CASE expr rule+ END { mk_case $2 $3 }
  | simple_expr simple_expr+ { mk_application $1 $2 }
  | expr_ op expr_ { mk_binop $1 $2 $3 }
  | expr_ COLONCOLON expr_ { mk_binop $1 Operators_Exp.Cons $3 }
  | LBRACK RBRACK { mk_empty_list }
;

simple_expr:
  LPAREN expr RPAREN { UHExp.Parenthesized($2) }
  | constant { $1 }
  | IDENT { UHExp.var $1 }
  | fn { $1 }
  | INJL LPAREN expr RPAREN { mk_inj_l $3 }
  | INJR LPAREN expr RPAREN { mk_inj_r $3 }
;

fn:
  LAMBDA pat PERIOD LBRACE expr RBRACE { mk_fn $2 $5 }
  | LAMBDA pat COLON typ PERIOD LBRACE expr RBRACE { mk_fn $2 $7 }
;

rule:
  BAR pat ARROW expr { mk_rule $2 $4 }
;

%inline op:
  PLUS { Operators_Exp.Plus }
  | MINUS { Operators_Exp.Minus }
  | MULT { Operators_Exp.Times }
  | DIV { Operators_Exp.Divide }
  | FPLUS { Operators_Exp.FPlus }
  | FMINUS { Operators_Exp.FMinus }
  | FMULT { Operators_Exp.FTimes }
  | FDIV { Operators_Exp.FDivide }
  | GREATER { Operators_Exp.GreaterThan }
  | LESSER { Operators_Exp.LessThan }
  | EQUAL EQUAL { Operators_Exp.Equals }
  | COMMA { Operators_Exp.Comma }
;

constant:
  INT { UHExp.intlit $1 }
  | FLOAT { UHExp.floatlit $1 }
  | TRUE { UHExp.boollit true }
  | FALSE { UHExp.boollit false }
;

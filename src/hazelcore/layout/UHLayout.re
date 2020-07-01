open Pretty;

[@deriving sexp]
type annot = UHAnnot.t;

type t = Layout.t(annot);

// TODO shouldn't need this, refactor to use option
module QueryResult = {
  type t('a) =
    | Stop
    | Skip
    | Return('a);

  let of_opt: option('a) => t('a) =
    fun
    | None => Stop
    | Some(a) => Return(a);
};

let rec contains = (query: annot => QueryResult.t(unit), l: t): bool => {
  let go = contains(query);
  switch (l) {
  | Linebreak
  | Text(_) => false
  | Align(l) => go(l)
  | Cat(l1, l2) => go(l1) || go(l2)
  | Annot(annot, l) =>
    switch (query(annot)) {
    | Stop => false
    | Skip => go(l)
    | Return () => true
    }
  };
};

let has_child =
  contains(
    fun
    | OpenChild(_)
    | ClosedChild(_) => Return()
    | _ => Skip,
  );

let has_inline_OpenChild =
  contains(
    fun
    | Step(_)
    | Tessera
    | LetLine => Skip
    | OpenChild({is_inline: true}) => Return()
    | _ => Stop,
  );

let has_para_OpenChild =
  contains(
    fun
    | Step(_)
    | Tessera
    | LetLine => Skip
    | OpenChild({is_inline: false}) => Return()
    | _ => Stop,
  );

type pos = {
  indent: int,
  row: int,
  col: int,
};

let pos_fold =
    (
      ~linebreak: pos => 'a,
      ~text: (pos, string) => 'a,
      ~align: (pos, 'a) => 'a,
      ~cat: (pos, 'a, 'a) => 'a,
      ~annot: (pos, UHAnnot.t, 'a) => 'a,
      l: t,
    ) => {
  let row = ref(0);
  let col = ref(0);
  let rec go = (~indent, l: t) => {
    let go' = go(~indent);
    let pos = {indent, row: row^, col: col^};
    switch (l) {
    | Linebreak =>
      row := row^ + 1;
      col := indent;
      linebreak(pos);
    | Text(s) =>
      col := col^ + StringUtil.utf8_length(s);
      text(pos, s);
    | Align(l) => align(pos, go(~indent=col^, l))
    | Cat(l1, l2) =>
      let a1 = go'(l1);
      let a2 = go'(l2);
      cat(pos, a1, a2);
    | Annot(ann, l) => annot(pos, ann, go'(l))
    };
  };
  go(~indent=0, l);
};

// TODO should be possible to make polymorphic over annot
// but was getting confusing type inference error
let rec find_and_decorate_Annot =
        (decorate: (annot, t) => QueryResult.t(t), l: t): option(t) => {
  let go = find_and_decorate_Annot(decorate);
  switch (l) {
  | Linebreak
  | Text(_) => None
  | Align(l1) => go(l1) |> OptUtil.map(l1 => Layout.Align(l1))
  | Cat(l1, l2) =>
    switch (go(l1)) {
    | Some(l1) => Some(Cat(l1, l2))
    | None => go(l2) |> OptUtil.map(l2 => Layout.Cat(l1, l2))
    }
  | Annot(annot, l1) =>
    switch (decorate(annot, l1)) {
    | Stop => None
    | Skip => go(l1) |> OptUtil.map(l1 => Layout.Annot(annot, l1))
    | Return(l) => Some(l)
    }
  };
};

let rec follow_steps_and_decorate =
        (~steps: CursorPath_common.steps, ~decorate: t => option(t), l: t)
        : option(t) => {
  let go = follow_steps_and_decorate(~decorate);
  switch (steps) {
  | [] => decorate(l)
  | [next_step, ...rest] =>
    l
    |> find_and_decorate_Annot((annot: UHAnnot.t, l: t) => {
         switch (annot) {
         | Step(step) when step == next_step =>
           l
           |> go(~steps=rest)
           |> OptUtil.map(l => Layout.Annot(annot, l))
           |> QueryResult.of_opt
         | OpenChild(_)
         | ClosedChild(_)
         | Tessera
         | LetLine
         | Term(_) => Skip
         | _ => Stop
         }
       })
  };
};

let find_and_decorate_caret =
    (~path as (steps, cursor): CursorPath_common.t, l: t): option(t) =>
  l
  |> follow_steps_and_decorate(
       ~steps,
       ~decorate=
         switch (cursor) {
         | OnText(j) =>
           find_and_decorate_Annot((annot, l) =>
             switch (annot) {
             | Token({shape: Text, _} as token_data) =>
               Return(
                 l
                 |> Layout.annot(
                      UHAnnot.Token({...token_data, has_cursor: Some(j)}),
                    ),
               )
             | EmptyLine
             | Term(_) => Skip
             | _ => Stop
             }
           )
         | OnOp(side) =>
           find_and_decorate_Annot((annot, l) =>
             switch (annot) {
             | Token({shape: Op, len, _} as token_data) =>
               Return(
                 l
                 |> Layout.annot(
                      UHAnnot.Token({
                        ...token_data,
                        has_cursor: Some(side == Before ? 0 : len),
                      }),
                    ),
               )
             | Tessera => Skip
             | _ => Stop
             }
           )
         | OnDelim(k, side) =>
           find_and_decorate_Annot((annot, l) =>
             switch (annot) {
             | Token({shape: Delim(k'), len, _} as token_data) when k' == k =>
               Return(
                 l
                 |> Layout.annot(
                      UHAnnot.Token({
                        ...token_data,
                        has_cursor: Some(side == Before ? 0 : len),
                      }),
                    ),
               )
             | Term(_)
             | Tessera
             | LetLine => Skip
             | _ => Stop
             }
           )
         },
     );

// TODO document difference from follow_steps_and_decorate
let rec find_and_decorate_Term =
        (
          ~steps: CursorPath_common.steps,
          ~decorate_Term: (UHAnnot.term_data, t) => t,
          l: t,
        )
        : option(t) => {
  let go = find_and_decorate_Term(~decorate_Term);
  switch (steps) {
  | [] =>
    l
    |> find_and_decorate_Annot((annot, l) =>
         switch (annot) {
         | Term(term_data) => Return(decorate_Term(term_data, l))
         | _ => Stop
         }
       )
  | [next_step, ...rest] =>
    l
    |> find_and_decorate_Annot((annot, l) => {
         let take_step = () =>
           l
           |> go(~steps=rest)
           |> OptUtil.map(l => Layout.Annot(annot, l))
           |> QueryResult.of_opt;
         let found_term_if = (cond, term_data) =>
           cond && rest == []
             ? QueryResult.Return(decorate_Term(term_data, l)) : Skip;
         switch (annot) {
         | Step(step) => step == next_step ? take_step() : Stop
         | Term({shape: SubBlock({hd_index, _}), _} as term_data) =>
           found_term_if(hd_index == next_step, term_data)
         | Term({shape: NTuple({comma_indices, _}), _} as term_data) =>
           found_term_if(comma_indices |> List.mem(next_step), term_data)
         | Term({shape: BinOp({op_index, _}), _} as term_data) =>
           found_term_if(op_index == next_step, term_data)
         | OpenChild(_)
         | ClosedChild(_)
         | Tessera
         | LetLine
         | Term({shape: Operand(_) | Case(_) | Rule, _}) => Skip
         | _ => Stop
         };
       })
  };
};

let find_and_decorate_cursor =
  find_and_decorate_Term(~decorate_Term=(term_data, l) =>
    l |> Layout.annot(UHAnnot.Term({...term_data, has_cursor: true}))
  );

let find_and_decorate_var_use =
  find_and_decorate_Term(~decorate_Term=(term_data, l) =>
    switch (term_data) {
    | {shape: Var(var_data), _} =>
      l
      |> Layout.annot(
           UHAnnot.Term({
             ...term_data,
             shape: Var({...var_data, show_use: true}),
           }),
         )
    | _ => failwith(__LOC__ ++ ": var not found")
    }
  );

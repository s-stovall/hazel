[@deriving sexp]
type elab_result_lines =
  | LinesElaborate(DHExp.t => DHExp.t, Context.t, Delta.t)
  | LinesDoNotElaborate;

module ElaborationResult = {
  [@deriving sexp]
  type t =
    | Elaborates(DHExp.t, HTyp.t, Delta.t)
    | DoesNotElaborate;

  let to_option =
    fun
    | DoesNotElaborate => None
    | Elaborates(pat, ty, delta) => Some((pat, ty, delta));

  let from_option =
    fun
    | None => DoesNotElaborate
    | Some((pat, ty, delta)) => Elaborates(pat, ty, delta);

  let bind = (x: t, ~f: ((DHExp.t, HTyp.t, Delta.t)) => t): t =>
    switch (x) {
    | DoesNotElaborate => DoesNotElaborate
    | Elaborates(dp, ty, delta) => f((dp, ty, delta))
    };
};

module Let_syntax = ElaborationResult;

let rec syn_elab =
        (ctx: Context.t, delta: Delta.t, e: UHExp.t): ElaborationResult.t =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("e", UHExp.sexp_of_t(e)),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () =>
    syn_elab_block(ctx, delta, e)
  )

and syn_elab_block =
    (ctx: Context.t, delta: Delta.t, block: UHExp.block): ElaborationResult.t =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("block", UHExp.sexp_of_block(block)),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () =>
    switch (block |> UHExp.Block.split_conclusion) {
    | None => DoesNotElaborate
    | Some((leading, conclusion)) =>
      switch (syn_elab_lines(ctx, delta, leading)) {
      | LinesDoNotElaborate => DoesNotElaborate
      | LinesElaborate(prelude, new_ctx, delta) =>
        switch (syn_elab_opseq(new_ctx, delta, conclusion)) {
        | DoesNotElaborate => DoesNotElaborate
        | Elaborates(d, ty, delta) =>
          let tyvars =
            Context.diff_tyvars(new_ctx, ctx)
            |> List.map(((idx, ty)) => (idx, ty));
          let ty = HTyp.subst_tyvars(ty, tyvars);
          Elaborates(prelude(d), ty, delta);
        }
      }
    }
  )

and syn_elab_lines =
    (ctx: Context.t, delta: Delta.t, lines: list(UHExp.line))
    : elab_result_lines =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("lines", Sexplib.Std.sexp_of_list(UHExp.sexp_of_line, lines)),
    ],
    ~result_sexp=sexp_of_elab_result_lines,
    () =>
    switch (lines) {
    | [] => LinesElaborate(d => d, ctx, delta)
    | [line, ...lines] =>
      switch (syn_elab_line(ctx, delta, line)) {
      | LinesDoNotElaborate => LinesDoNotElaborate
      | LinesElaborate(prelude_line, ctx, delta) =>
        switch (syn_elab_lines(ctx, delta, lines)) {
        | LinesDoNotElaborate => LinesDoNotElaborate
        | LinesElaborate(prelude_lines, ctx, delta) =>
          LinesElaborate(d => prelude_line(prelude_lines(d)), ctx, delta)
        }
      }
    }
  )

and syn_elab_line =
    (ctx: Context.t, delta: Delta.t, line: UHExp.line): elab_result_lines =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("line", UHExp.sexp_of_line(line)),
    ],
    ~result_sexp=sexp_of_elab_result_lines,
    () =>
    switch (line) {
    | ExpLine(e1) =>
      switch (syn_elab_opseq(ctx, delta, e1)) {
      | DoesNotElaborate => LinesDoNotElaborate
      | Elaborates(d1, _, delta) =>
        let prelude = d2 => DHExp.Let(Wild, d1, d2);
        LinesElaborate(prelude, ctx, delta);
      }
    | EmptyLine
    | CommentLine(_) => LinesElaborate(d => d, ctx, delta)
    | LetLine(p, def) =>
      switch (Statics_Pat.syn(ctx, p)) {
      | None => LinesDoNotElaborate
      | Some((ty_p, ctx_p)) =>
        /* let recursive_id = Statics_Exp.recursive_let_id(ctx, p, def); */
        Log.debug_states(
          __FUNCTION__,
          [
            ("ctx", Context.sexp_of_t(ctx)),
            ("ctx_p", Context.sexp_of_t(ctx_p)),
            ("ty_p", HTyp.sexp_of_t(ty_p)),
          ],
        );
        switch (ana_elab(ctx_p, delta, def, ty_p)) {
        | DoesNotElaborate => LinesDoNotElaborate
        | Elaborates(d, ty_p', delta') =>
          Log.debug_states(
            __FUNCTION__,
            [
              ("d", DHExp.sexp_of_t(d)),
              ("ty_p''", HTyp.sexp_of_t(ty_p')),
              ("delta'", Delta.sexp_of_t(delta')),
            ],
          );
          let dty_p = (ctx_p, ty_p);
          let dty_p' = (ctx_p, ty_p');
          /* let d1 = */
          /*   switch (recursive_id) { */
          /*   | None => d1 */
          /*   | Some(x) => */
          /*     let d1 = DHExp.cast(BoundVar(x), dty1', dty1); */
          /*     FixF(x, dty1', Evaluator.subst_var(d1, x, d1)); */
          /*   }; */
          /* let d1 = DHExp.cast(d1, dty1', dty1); */
          Log.debug_states(
            __FUNCTION__,
            [
              ("dty_p", DHTyp.sexp_of_t(dty_p)),
              ("dty_p'", DHTyp.sexp_of_t(dty_p')),
              ("d", DHExp.sexp_of_t(d)),
            ],
          );
          switch (Elaborator_Pat.ana_elab(ctx_p, delta', p, ty_p)) {
          | DoesNotElaborate => LinesDoNotElaborate
          | Elaborates(dp, _, ctx, delta) =>
            let prelude = d2 => DHExp.Let(dp, d, d2);
            LinesElaborate(prelude, ctx, delta);
          };
        };
      }
    | TyAliasLine(tp, ty1) =>
      switch (Elaborator_Typ.syn_elab(ctx, delta, ty1)) {
      | None => LinesDoNotElaborate
      | Some((ty1, k, delta)) =>
        let ctx1 = Statics_TPat.matches(ctx, tp, ty1, k);
        let dty = (ctx, ty1);
        let prelude = d => DHExp.TyAlias(tp, dty, d);
        LinesElaborate(prelude, ctx1, delta);
      }
    }
  )

and syn_elab_opseq =
    (ctx: Context.t, delta: Delta.t, OpSeq(skel, seq): UHExp.opseq)
    : ElaborationResult.t =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("opseq", UHExp.sexp_of_opseq(OpSeq(skel, seq))),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () =>
    syn_elab_skel(ctx, delta, skel, seq)
  )

and syn_elab_skel =
    (ctx: Context.t, delta: Delta.t, skel: UHExp.skel, seq: UHExp.seq)
    : ElaborationResult.t =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("opseq", UHExp.sexp_of_opseq(OpSeq(skel, seq))),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () =>
    switch (skel) {
    | Placeholder(n) =>
      let en = seq |> Seq.nth_operand(n);
      syn_elab_operand(ctx, delta, en);
    | BinOp(InHole(TypeInconsistent as reason, u), op, skel1, skel2)
    | BinOp(InHole(WrongLength as reason, u), Comma as op, skel1, skel2) =>
      let skel_not_in_hole = Skel.BinOp(NotInHole, op, skel1, skel2);
      switch (syn_elab_skel(ctx, delta, skel_not_in_hole, seq)) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d, _, delta) =>
        let sigma = Environment.id_env(ctx);
        let delta =
          MetaVarMap.add(u, Delta.Hole.Expression(HTyp.hole(), ctx), delta);
        Elaborates(NonEmptyHole(reason, u, 0, sigma, d), HTyp.hole(), delta);
      };
    | BinOp(InHole(WrongLength, _), _, _, _) => DoesNotElaborate
    | BinOp(NotInHole, Space, skel1, skel2) =>
      switch (Statics_Exp.syn_skel(ctx, skel1, seq)) {
      | None => DoesNotElaborate
      | Some(ty1) =>
        switch (HTyp.matched_arrow(ctx, ty1)) {
        | None => DoesNotElaborate
        | Some((ty2, ty)) =>
          let ty2_arrow_ty = HTyp.arrow(ty2, ty);
          switch (ana_elab_skel(ctx, delta, skel1, seq, ty2_arrow_ty)) {
          | DoesNotElaborate => DoesNotElaborate
          | Elaborates(d1, ty1', delta) =>
            switch (ana_elab_skel(ctx, delta, skel2, seq, ty2)) {
            | DoesNotElaborate => DoesNotElaborate
            | Elaborates(d2, ty2', delta) =>
              let dc1 = DHExp.cast(d1, (ctx, ty1'), (ctx, ty2_arrow_ty));
              let dc2 = DHExp.cast(d2, (ctx, ty2'), (ctx, ty2));
              let d = DHExp.Ap(dc1, dc2);
              Elaborates(d, ty, delta);
            }
          };
        }
      }
    | BinOp(NotInHole, Comma, _, _) =>
      switch (UHExp.get_tuple_elements(skel)) {
      | [skel1, skel2, ...tail] =>
        let%bind (dp1, ty1, delta) = syn_elab_skel(ctx, delta, skel1, seq);
        let%bind (dp2, ty2, delta) = syn_elab_skel(ctx, delta, skel2, seq);
        tail
        |> ListUtil.map_with_accumulator_opt(
             ((dp_acc, delta), skel) => {
               syn_elab_skel(ctx, delta, skel, seq)
               |> ElaborationResult.to_option
               |> Option.map(((dp, ty, delta)) =>
                    ((DHExp.Pair(dp_acc, dp), delta), ty)
                  )
             },
             (DHExp.Pair(dp1, dp2), delta),
           )
        |> Option.map((((dp_acc, delta), tys)) =>
             (dp_acc, HTyp.product([ty1, ty2, ...tys]), delta)
           )
        |> ElaborationResult.from_option;
      | _ =>
        raise(
          Invalid_argument(
            "Encountered tuple pattern type with less than 2 elements!",
          ),
        )
      }
    | BinOp(NotInHole, Cons, skel1, skel2) =>
      switch (syn_elab_skel(ctx, delta, skel1, seq)) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d1, ty1, delta) =>
        let ty = HTyp.list(ty1);
        switch (ana_elab_skel(ctx, delta, skel2, seq, ty)) {
        | DoesNotElaborate => DoesNotElaborate
        | Elaborates(d2, ty2, delta) =>
          let d2c = DHExp.cast(d2, (ctx, ty2), (ctx, ty));
          let d = DHExp.Cons(d1, d2c);
          Elaborates(d, ty, delta);
        };
      }
    | BinOp(NotInHole, (Plus | Minus | Times | Divide) as op, skel1, skel2)
    | BinOp(NotInHole, (LessThan | GreaterThan | Equals) as op, skel1, skel2) =>
      switch (ana_elab_skel(ctx, delta, skel1, seq, HTyp.int())) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d1, ty1, delta) =>
        switch (ana_elab_skel(ctx, delta, skel2, seq, HTyp.int())) {
        | DoesNotElaborate => DoesNotElaborate
        | Elaborates(d2, ty2, delta) =>
          let dc1 =
            DHExp.cast(d1, (ctx, ty1), (Context.initial, HTyp.int()));
          let dc2 =
            DHExp.cast(d2, (ctx, ty2), (Context.initial, HTyp.int()));
          switch (DHExp.BinIntOp.of_op(op)) {
          | None => DoesNotElaborate
          | Some((op, ty)) =>
            let d = DHExp.BinIntOp(op, dc1, dc2);
            Elaborates(d, ty, delta);
          };
        }
      }
    | BinOp(
        NotInHole,
        (FPlus | FMinus | FTimes | FDivide) as op,
        skel1,
        skel2,
      )
    | BinOp(
        NotInHole,
        (FLessThan | FGreaterThan | FEquals) as op,
        skel1,
        skel2,
      ) =>
      switch (ana_elab_skel(ctx, delta, skel1, seq, HTyp.float())) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d1, ty1, delta) =>
        switch (ana_elab_skel(ctx, delta, skel2, seq, HTyp.float())) {
        | DoesNotElaborate => DoesNotElaborate
        | Elaborates(d2, ty2, delta) =>
          let dc1 =
            DHExp.cast(d1, (ctx, ty1), (Context.initial, HTyp.float()));
          let dc2 =
            DHExp.cast(d2, (ctx, ty2), (Context.initial, HTyp.float()));
          switch (DHExp.BinFloatOp.of_op(op)) {
          | None => DoesNotElaborate
          | Some((op, ty)) =>
            let d = DHExp.BinFloatOp(op, dc1, dc2);
            Elaborates(d, ty, delta);
          };
        }
      }
    | BinOp(NotInHole, (And | Or) as op, skel1, skel2) =>
      switch (ana_elab_skel(ctx, delta, skel1, seq, HTyp.bool())) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d1, ty1, delta) =>
        switch (ana_elab_skel(ctx, delta, skel2, seq, HTyp.bool())) {
        | DoesNotElaborate => DoesNotElaborate
        | Elaborates(d2, ty2, delta) =>
          let dc1 =
            DHExp.cast(d1, (ctx, ty1), (Context.initial, HTyp.bool()));
          let dc2 =
            DHExp.cast(d2, (ctx, ty2), (Context.initial, HTyp.bool()));
          switch (DHExp.BinBoolOp.of_op(op)) {
          | None => DoesNotElaborate
          | Some(op) =>
            let d = DHExp.BinBoolOp(op, dc1, dc2);
            Elaborates(d, HTyp.bool(), delta);
          };
        }
      }
    }
  )

and syn_elab_operand =
    (ctx: Context.t, delta: Delta.t, operand: UHExp.operand)
    : ElaborationResult.t =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("operand", UHExp.sexp_of_operand(operand)),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () =>
    switch (operand) {
    /* in hole */
    | Var(InHole(TypeInconsistent as reason, u), _, _)
    | IntLit(InHole(TypeInconsistent as reason, u), _)
    | FloatLit(InHole(TypeInconsistent as reason, u), _)
    | BoolLit(InHole(TypeInconsistent as reason, u), _)
    | ListNil(InHole(TypeInconsistent as reason, u))
    | Fun(InHole(TypeInconsistent as reason, u), _, _)
    | Inj(InHole(TypeInconsistent as reason, u), _, _)
    | Case(StandardErrStatus(InHole(TypeInconsistent as reason, u)), _, _) =>
      let operand' = operand |> UHExp.set_err_status_operand(NotInHole);
      switch (syn_elab_operand(ctx, delta, operand')) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d, _, delta) =>
        let sigma = Environment.id_env(ctx);
        let delta =
          MetaVarMap.add(u, Delta.Hole.Expression(HTyp.hole(), ctx), delta);
        Elaborates(NonEmptyHole(reason, u, 0, sigma, d), HTyp.hole(), delta);
      };
    | Var(InHole(WrongLength, _), _, _)
    | IntLit(InHole(WrongLength, _), _)
    | FloatLit(InHole(WrongLength, _), _)
    | BoolLit(InHole(WrongLength, _), _)
    | ListNil(InHole(WrongLength, _))
    | Fun(InHole(WrongLength, _), _, _)
    | Inj(InHole(WrongLength, _), _, _)
    | Case(StandardErrStatus(InHole(WrongLength, _)), _, _) =>
      DoesNotElaborate
    | Case(InconsistentBranches(rule_types, u), scrut, rules) =>
      switch (syn_elab(ctx, delta, scrut)) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d1, pat_ty, delta) =>
        let elab_rules =
          List.fold_left2(
            (b, r_t, r) =>
              switch (b) {
              | None => None
              | Some((drs, delta)) =>
                switch (syn_elab_rule(ctx, delta, r, pat_ty, r_t)) {
                | None => None
                | Some((dr, delta)) =>
                  let drs = drs @ [dr];
                  Some((drs, delta));
                }
              },
            Some(([], delta)),
            rule_types,
            rules,
          );
        switch (elab_rules) {
        | None => DoesNotElaborate
        | Some((drs, delta)) =>
          let sigma = Environment.id_env(ctx);
          let delta =
            MetaVarMap.add(
              u,
              Delta.Hole.Expression(HTyp.hole(), ctx),
              delta,
            );
          let d = DHExp.Case(d1, drs, 0);
          Elaborates(
            InconsistentBranches(u, 0, sigma, d),
            HTyp.hole(),
            delta,
          );
        };
      } /* not in hole */
    | EmptyHole(u) =>
      let sigma = Environment.id_env(ctx);
      let d = DHExp.EmptyHole(u, 0, sigma);
      let ty = HTyp.hole();
      let delta = MetaVarMap.add(u, Delta.Hole.Expression(ty, ctx), delta);
      Elaborates(d, ty, delta);
    | InvalidText(u, t) =>
      let sigma = Environment.id_env(ctx);
      let d = DHExp.InvalidText(u, 0, sigma, t);
      let ty = HTyp.hole();
      let delta = MetaVarMap.add(u, Delta.Hole.Expression(ty, ctx), delta);
      Elaborates(d, ty, delta);
    | Var(NotInHole, NotInVarHole, x) =>
      switch (Context.var_type(ctx, x)) {
      | Some(ty) => Elaborates(BoundVar(x), ty, delta)
      | None => DoesNotElaborate
      }
    | Var(NotInHole, InVarHole(reason, u), x) =>
      let sigma = Environment.id_env(ctx);
      let delta =
        MetaVarMap.add(u, Delta.Hole.Expression(HTyp.hole(), ctx), delta);
      let d =
        switch (reason) {
        | Free => DHExp.FreeVar(u, 0, sigma, x)
        | Keyword(k) => DHExp.Keyword(u, 0, sigma, k)
        };
      Elaborates(d, HTyp.hole(), delta);
    | IntLit(NotInHole, n) =>
      switch (int_of_string_opt(n)) {
      | Some(n) => Elaborates(IntLit(n), HTyp.int(), delta)
      | None => DoesNotElaborate
      }
    | FloatLit(NotInHole, f) =>
      switch (TextShape.hazel_float_of_string_opt(f)) {
      | Some(f) => Elaborates(FloatLit(f), HTyp.float(), delta)
      | None => DoesNotElaborate
      }
    | BoolLit(NotInHole, b) => Elaborates(BoolLit(b), HTyp.bool(), delta)
    | ListNil(NotInHole) =>
      let elt_ty = HTyp.hole();
      Elaborates(ListNil((ctx, elt_ty)), HTyp.list(elt_ty), delta);
    | Parenthesized(body) => syn_elab(ctx, delta, body)
    | Fun(NotInHole, p, body) =>
      switch (Elaborator_Pat.syn_elab(ctx, delta, p)) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(dp, ty1, ctx, delta) =>
        switch (syn_elab(ctx, delta, body)) {
        | DoesNotElaborate => DoesNotElaborate
        | Elaborates(d1, ty2, delta) =>
          let d = DHExp.Fun(dp, (ctx, ty1), d1);
          Elaborates(d, HTyp.arrow(ty1, ty2), delta);
        }
      }
    | Inj(NotInHole, side, body) =>
      switch (syn_elab(ctx, delta, body)) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d1, ty1, delta) =>
        let d = DHExp.Inj((ctx, HTyp.hole()), side, d1);
        let ty =
          switch (side) {
          | L => HTyp.sum(ty1, HTyp.hole())
          | R => HTyp.sum(HTyp.hole(), ty1)
          };
        Elaborates(d, ty, delta);
      }
    | Case(StandardErrStatus(NotInHole), scrut, rules) =>
      switch (syn_elab(ctx, delta, scrut)) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d1, ty, delta) =>
        switch (syn_elab_rules(ctx, delta, rules, ty)) {
        | None => DoesNotElaborate
        | Some((drs, glb, delta)) =>
          let d = DHExp.ConsistentCase(DHExp.Case(d1, drs, 0));
          Elaborates(d, glb, delta);
        }
      }
    }
  )

and syn_elab_rules =
    (ctx: Context.t, delta: Delta.t, rules: list(UHExp.rule), pat_ty: HTyp.t)
    : option((list(DHExp.rule), HTyp.t, Delta.t)) =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("rules", UHExp.sexp_of_rules(rules)),
      ("pat_ty", HTyp.sexp_of_t(pat_ty)),
    ],
    ~result_sexp=
      Sexplib.Std.sexp_of_option(((rules, ty, delta)) =>
        List([
          Sexplib.Std.sexp_of_list(DHExp.sexp_of_rule, rules),
          HTyp.sexp_of_t(ty),
          Delta.sexp_of_t(delta),
        ])
      ),
    () =>
    switch (Statics_Exp.syn_rules(ctx, rules, pat_ty)) {
    | None => None
    | Some(glb) =>
      let elabed_rule_info =
        List.fold_left(
          (b, r) =>
            switch (b) {
            | None => None
            | Some((drs, delta)) =>
              switch (syn_elab_rule(ctx, delta, r, pat_ty, glb)) {
              | None => None
              | Some((dr, delta)) =>
                let drs = drs @ [dr];
                Some((drs, delta));
              }
            },
          Some(([], delta)),
          rules,
        );
      switch (elabed_rule_info) {
      | None => None
      | Some((drs, delta)) => Some((drs, glb, delta))
      };
    }
  )

and syn_elab_rule =
    (
      ctx: Context.t,
      delta: Delta.t,
      r: UHExp.rule,
      pat_ty: HTyp.t,
      clause_ty: HTyp.t,
    )
    : option((DHExp.rule, Delta.t)) => {
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("r", UHExp.sexp_of_rule(r)),
      ("pat_ty", HTyp.sexp_of_t(pat_ty)),
      ("clause_ty", HTyp.sexp_of_t(clause_ty)),
    ],
    ~result_sexp=
      Sexplib.Std.sexp_of_option(((r, delta)) =>
        List([DHExp.sexp_of_rule(r), Delta.sexp_of_t(delta)])
      ),
    () => {
      let UHExp.Rule(p, clause) = r;
      switch (Elaborator_Pat.ana_elab(ctx, delta, p, pat_ty)) {
      | DoesNotElaborate => None
      | Elaborates(dp, _, ctx, delta) =>
        switch (syn_elab(ctx, delta, clause)) {
        | DoesNotElaborate => None
        | Elaborates(d1, ty1, delta) =>
          Some((
            Rule(dp, DHExp.cast(d1, (ctx, ty1), (ctx, clause_ty))),
            delta,
          ))
        }
      };
    },
  );
}

and ana_elab =
    (ctx: Context.t, delta: Delta.t, e: UHExp.t, ty: HTyp.t)
    : ElaborationResult.t =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("e", UHExp.sexp_of_t(e)),
      ("ty", HTyp.sexp_of_t(ty)),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () =>
    ana_elab_block(ctx, delta, e, ty)
  )

and ana_elab_block =
    (ctx: Context.t, delta: Delta.t, block: UHExp.block, ty: HTyp.t)
    : ElaborationResult.t =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("block", UHExp.sexp_of_block(block)),
      ("ty", HTyp.sexp_of_t(ty)),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () =>
    switch (block |> UHExp.Block.split_conclusion) {
    | None => DoesNotElaborate
    | Some((leading, conclusion)) =>
      switch (syn_elab_lines(ctx, delta, leading)) {
      | LinesDoNotElaborate => DoesNotElaborate
      | LinesElaborate(prelude, new_ctx, delta) =>
        Log.debug_states(
          __FUNCTION__,
          [
            ("new_ctx", Context.sexp_of_t(new_ctx)),
            ("delta", Delta.sexp_of_t(delta)),
          ],
        );
        Log.debug_states(__FUNCTION__, [("ty", HTyp.sexp_of_t(ty))]);
        switch (ana_elab_opseq(new_ctx, delta, conclusion, ty)) {
        | DoesNotElaborate => DoesNotElaborate
        | Elaborates(d, ty, delta) => Elaborates(prelude(d), ty, delta)
        };
      }
    }
  )

and ana_elab_opseq =
    (
      ctx: Context.t,
      delta: Delta.t,
      OpSeq(skel, seq) as opseq: UHExp.opseq,
      ty: HTyp.t,
    )
    : ElaborationResult.t => {
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("opseq", UHExp.sexp_of_opseq(opseq)),
      ("ty", HTyp.sexp_of_t(ty)),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () => {
      let ty_h = HTyp.head_normalize(ctx, ty);
      // handle n-tuples
      switch (Statics_Exp.tuple_zip(skel, ty_h)) {
      | Some(skel_tys) =>
        skel_tys
        |> List.fold_left(
             (
               acc: option((list(DHExp.t), list(HTyp.t), Delta.t)),
               (skel: UHExp.skel, ty: HTyp.t),
             ) =>
               switch (acc) {
               | None => None
               | Some((rev_ds, rev_tys, delta)) =>
                 switch (ana_elab_skel(ctx, delta, skel, seq, ty)) {
                 | DoesNotElaborate => None
                 | Elaborates(d, ty, delta) =>
                   Some(([d, ...rev_ds], [ty, ...rev_tys], delta))
                 }
               },
             Some(([], [], delta)),
           )
        |> (
          fun
          | None => ElaborationResult.DoesNotElaborate
          | Some((rev_ds, rev_tys, delta)) => {
              let d = rev_ds |> List.rev |> DHExp.mk_tuple;
              let ty =
                switch (rev_tys) {
                | [] => failwith("expected at least 1 element")
                | [ty] => ty
                | _ => HTyp.product(rev_tys |> List.rev)
                };
              Elaborates(d, ty, delta);
            }
        )
      | None =>
        if (List.length(HTyp.get_prod_elements(ty_h)) == 1) {
          skel
          |> UHExp.get_tuple_elements
          |> List.fold_left(
               (
                 acc: option((list(DHExp.t), list(HTyp.t), Delta.t)),
                 skel: UHExp.skel,
               ) =>
                 switch (acc) {
                 | None => None
                 | Some((rev_ds, rev_tys, delta)) =>
                   switch (syn_elab_skel(ctx, delta, skel, seq)) {
                   | DoesNotElaborate => None
                   | Elaborates(d, ty, delta) =>
                     Some(([d, ...rev_ds], [ty, ...rev_tys], delta))
                   }
                 },
               Some(([], [], delta)),
             )
          |> (
            fun
            | None => ElaborationResult.DoesNotElaborate
            | Some((rev_ds, rev_tys, delta)) => {
                let d = DHExp.mk_tuple(List.rev(rev_ds));
                let ty =
                  switch (rev_tys) {
                  | [] => failwith("expected at least 1 element")
                  | [ty] => ty
                  | _ => HTyp.product(rev_tys |> List.rev)
                  };
                Elaborates(d, ty, delta);
              }
          );
        } else {
          switch (opseq |> UHExp.get_err_status_opseq) {
          | NotInHole
          | InHole(TypeInconsistent, _) => DoesNotElaborate
          | InHole(WrongLength as reason, u) =>
            switch (
              syn_elab_opseq(
                ctx,
                delta,
                opseq |> UHExp.set_err_status_opseq(NotInHole),
              )
            ) {
            | DoesNotElaborate => DoesNotElaborate
            | Elaborates(d, _, delta) =>
              let sigma = Environment.id_env(ctx);
              let delta =
                MetaVarMap.add(u, Delta.Hole.Expression(ty, ctx), delta);
              Elaborates(
                NonEmptyHole(reason, u, 0, sigma, d),
                HTyp.hole(),
                delta,
              );
            }
          };
        }
      };
    },
  );
}

and ana_elab_skel =
    (
      ctx: Context.t,
      delta: Delta.t,
      skel: UHExp.skel,
      seq: UHExp.seq,
      ty: HTyp.t,
    )
    : ElaborationResult.t =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("skel", UHExp.sexp_of_skel(skel)),
      ("seq", UHExp.sexp_of_seq(seq)),
      ("ty", HTyp.sexp_of_t(ty)),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () =>
    switch (skel) {
    | BinOp(_, Comma, _, _)
    | BinOp(InHole(WrongLength, _), _, _, _) =>
      // tuples handled at opseq level
      DoesNotElaborate
    | Placeholder(n) =>
      let en = seq |> Seq.nth_operand(n);
      ana_elab_operand(ctx, delta, en, ty);
    | BinOp(InHole(TypeInconsistent as reason, u), op, skel1, skel2) =>
      let skel_not_in_hole = Skel.BinOp(NotInHole, op, skel1, skel2);
      switch (syn_elab_skel(ctx, delta, skel_not_in_hole, seq)) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d1, _, delta) =>
        let sigma = Environment.id_env(ctx);
        let delta = MetaVarMap.add(u, Delta.Hole.Expression(ty, ctx), delta);
        let d = DHExp.NonEmptyHole(reason, u, 0, sigma, d1);
        Elaborates(d, HTyp.hole(), delta);
      };
    | BinOp(NotInHole, Cons, skel1, skel2) =>
      switch (HTyp.matched_list(ctx, ty)) {
      | None => DoesNotElaborate
      | Some(ty_elt) =>
        switch (ana_elab_skel(ctx, delta, skel1, seq, ty_elt)) {
        | DoesNotElaborate => DoesNotElaborate
        | Elaborates(d1, ty_elt', delta) =>
          let d1c = DHExp.cast(d1, (ctx, ty_elt'), (ctx, ty_elt));
          let ty_list = HTyp.list(ty_elt);
          switch (ana_elab_skel(ctx, delta, skel2, seq, ty_list)) {
          | DoesNotElaborate => DoesNotElaborate
          | Elaborates(d2, ty2, delta) =>
            let d2c = DHExp.cast(d2, (ctx, ty2), (ctx, ty_list));
            let d = DHExp.Cons(d1c, d2c);
            Elaborates(d, ty_list, delta);
          };
        }
      }
    | BinOp(
        _,
        Plus | Minus | Times | Divide | FPlus | FMinus | FTimes | FDivide |
        LessThan |
        GreaterThan |
        Equals |
        FLessThan |
        FGreaterThan |
        FEquals |
        And |
        Or |
        Space,
        _,
        _,
      ) =>
      switch (syn_elab_skel(ctx, delta, skel, seq)) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d, ty', delta) =>
        if (HTyp.consistent(ctx, ty, ty')) {
          Elaborates(d, ty', delta);
        } else {
          DoesNotElaborate;
        }
      }
    }
  )

and ana_elab_operand =
    (ctx: Context.t, delta: Delta.t, operand: UHExp.operand, ty: HTyp.t)
    : ElaborationResult.t =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("operand", UHExp.sexp_of_operand(operand)),
      ("ty", HTyp.sexp_of_t(ty)),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () =>
    switch (operand) {
    /* in hole */
    | Var(InHole(TypeInconsistent as reason, u), _, _)
    | IntLit(InHole(TypeInconsistent as reason, u), _)
    | FloatLit(InHole(TypeInconsistent as reason, u), _)
    | BoolLit(InHole(TypeInconsistent as reason, u), _)
    | ListNil(InHole(TypeInconsistent as reason, u))
    | Fun(InHole(TypeInconsistent as reason, u), _, _)
    | Inj(InHole(TypeInconsistent as reason, u), _, _)
    | Case(StandardErrStatus(InHole(TypeInconsistent as reason, u)), _, _) =>
      let operand' = operand |> UHExp.set_err_status_operand(NotInHole);
      switch (syn_elab_operand(ctx, delta, operand')) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d, _, delta) =>
        let sigma = Environment.id_env(ctx);
        let delta = MetaVarMap.add(u, Delta.Hole.Expression(ty, ctx), delta);
        Elaborates(NonEmptyHole(reason, u, 0, sigma, d), HTyp.hole(), delta);
      };
    | Case(InconsistentBranches(_, u), _, _) =>
      switch (syn_elab_operand(ctx, delta, operand)) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d, e_ty, delta) =>
        let delta = MetaVarMap.add(u, Delta.Hole.Expression(ty, ctx), delta);
        Elaborates(d, e_ty, delta);
      }
    | Var(InHole(WrongLength, _), _, _)
    | IntLit(InHole(WrongLength, _), _)
    | FloatLit(InHole(WrongLength, _), _)
    | BoolLit(InHole(WrongLength, _), _)
    | ListNil(InHole(WrongLength, _))
    | Fun(InHole(WrongLength, _), _, _)
    | Inj(InHole(WrongLength, _), _, _)
    | Case(StandardErrStatus(InHole(WrongLength, _)), _, _) =>
      DoesNotElaborate /* not in hole */
    | EmptyHole(u) =>
      let sigma = Environment.id_env(ctx);
      let d = DHExp.EmptyHole(u, 0, sigma);
      let delta = MetaVarMap.add(u, Delta.Hole.Expression(ty, ctx), delta);
      Elaborates(d, ty, delta);
    | Var(NotInHole, InVarHole(reason, u), x) =>
      let sigma = Environment.id_env(ctx);
      let delta = MetaVarMap.add(u, Delta.Hole.Expression(ty, ctx), delta);
      let d: DHExp.t =
        switch (reason) {
        | Free => FreeVar(u, 0, sigma, x)
        | Keyword(k) => Keyword(u, 0, sigma, k)
        };
      Elaborates(d, ty, delta);
    | Parenthesized(body) => ana_elab(ctx, delta, body, ty)
    | Fun(NotInHole, p, body) =>
      switch (HTyp.matched_arrow(ctx, ty)) {
      | None => DoesNotElaborate
      | Some((ty1_given, ty2)) =>
        let ty1_ann =
          switch (Statics_Pat.syn(ctx, p)) {
          | None => ty1_given
          | Some((ty_p, _)) => ty_p
          };
        switch (HTyp.consistent(ctx, ty1_ann, ty1_given)) {
        | false => DoesNotElaborate
        | true =>
          switch (Elaborator_Pat.ana_elab(ctx, delta, p, ty1_ann)) {
          | DoesNotElaborate => DoesNotElaborate
          | Elaborates(dp, ty1p, ctx, delta) =>
            switch (ana_elab(ctx, delta, body, ty2)) {
            | DoesNotElaborate => DoesNotElaborate
            | Elaborates(d1, ty2, delta) =>
              let ty = HTyp.arrow(ty1p, ty2);
              let d = DHExp.Fun(dp, (ctx, ty1p), d1);
              Elaborates(d, ty, delta);
            }
          }
        };
      }
    | Inj(NotInHole, side, body) =>
      switch (HTyp.matched_sum(ctx, ty)) {
      | None => DoesNotElaborate
      | Some((ty1, ty2)) =>
        let e1ty = InjSide.pick(side, ty1, ty2);
        switch (ana_elab(ctx, delta, body, e1ty)) {
        | DoesNotElaborate => DoesNotElaborate
        | Elaborates(d1, e1ty', delta) =>
          let (ann_ty, ty) =
            switch (side) {
            | L => (ty2, HTyp.sum(e1ty', ty2))
            | R => (ty1, HTyp.sum(ty1, e1ty'))
            };
          let d = DHExp.Inj((ctx, ann_ty), side, d1);
          Elaborates(d, ty, delta);
        };
      }
    | Case(StandardErrStatus(NotInHole), scrut, rules) =>
      switch (syn_elab(ctx, delta, scrut)) {
      | DoesNotElaborate => DoesNotElaborate
      | Elaborates(d1, ty1, delta) =>
        switch (ana_elab_rules(ctx, delta, rules, ty1, ty)) {
        | None => DoesNotElaborate
        | Some((drs, delta)) =>
          let d = DHExp.ConsistentCase(DHExp.Case(d1, drs, 0));
          Elaborates(d, ty, delta);
        }
      }
    | ListNil(NotInHole) =>
      switch (HTyp.matched_list(ctx, ty)) {
      | None => DoesNotElaborate
      | Some(elt_ty) =>
        Elaborates(ListNil((ctx, elt_ty)), HTyp.list(elt_ty), delta)
      }
    | InvalidText(u, t) =>
      let sigma = Environment.id_env(ctx);
      let d = DHExp.InvalidText(u, 0, sigma, t);
      let delta = MetaVarMap.add(u, Delta.Hole.Expression(ty, ctx), delta);
      Elaborates(d, ty, delta);
    | Var(NotInHole, NotInVarHole, _)
    | BoolLit(NotInHole, _)
    | IntLit(NotInHole, _)
    | FloatLit(NotInHole, _) =>
      /* subsumption */
      syn_elab_operand(ctx, delta, operand)
    }
  )

and ana_elab_rules =
    (
      ctx: Context.t,
      delta: Delta.t,
      rules: list(UHExp.rule),
      pat_ty: HTyp.t,
      clause_ty: HTyp.t,
    )
    : option((list(DHExp.rule), Delta.t)) =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("rules", UHExp.sexp_of_rules(rules)),
      ("pat_ty", HTyp.sexp_of_t(pat_ty)),
      ("clause_ty", HTyp.sexp_of_t(clause_ty)),
    ],
    ~result_sexp=
      Sexplib.Std.sexp_of_option(((rules, delta)) =>
        List([
          Sexplib.Std.sexp_of_list(DHExp.sexp_of_rule, rules),
          Delta.sexp_of_t(delta),
        ])
      ),
    () =>
    rules
    |> List.fold_left(
         (b, r) =>
           switch (b) {
           | None => None
           | Some((drs, delta)) =>
             switch (ana_elab_rule(ctx, delta, r, pat_ty, clause_ty)) {
             | None => None
             | Some((dr, delta)) =>
               let drs = drs @ [dr];
               Some((drs, delta));
             }
           },
         Some(([], delta)),
       )
  )

and ana_elab_rule =
    (
      ctx: Context.t,
      delta: Delta.t,
      r: UHExp.rule,
      pat_ty: HTyp.t,
      clause_ty: HTyp.t,
    )
    : option((DHExp.rule, Delta.t)) => {
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("r", UHExp.sexp_of_rule(r)),
      ("pat_ty", HTyp.sexp_of_t(pat_ty)),
      ("clause_ty", HTyp.sexp_of_t(clause_ty)),
    ],
    ~result_sexp=
      Sexplib.Std.sexp_of_option(((r, delta)) =>
        List([DHExp.sexp_of_rule(r), Delta.sexp_of_t(delta)])
      ),
    () => {
      let UHExp.Rule(p, clause) = r;
      switch (Elaborator_Pat.ana_elab(ctx, delta, p, pat_ty)) {
      | DoesNotElaborate => None
      | Elaborates(dp, _, ctx, delta) =>
        switch (ana_elab(ctx, delta, clause, clause_ty)) {
        | DoesNotElaborate => None
        | Elaborates(d1, ty1, delta) =>
          Some((
            Rule(dp, DHExp.cast(d1, (ctx, ty1), (ctx, clause_ty))),
            delta,
          ))
        }
      };
    },
  );
};

/* Bind built-ins before an elaborated expression. */
let elab_wrap_builtins = (d: DHExp.t): DHExp.t =>
  Log.debug_function(
    __FUNCTION__,
    [("d", DHExp.sexp_of_t(d))],
    ~result_sexp=DHExp.sexp_of_t,
    () =>
    List.fold_left(
      (d', (ident, (_, elab))) => DHExp.Let(Var(ident), elab, d'),
      d,
      Builtins.forms,
    )
  );

let elab = (ctx: Context.t, delta: Delta.t, e: UHExp.t): ElaborationResult.t => {
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("delta", Delta.sexp_of_t(delta)),
      ("e", UHExp.sexp_of_t(e)),
    ],
    ~result_sexp=ElaborationResult.sexp_of_t,
    () =>
    switch (syn_elab(ctx, delta, e)) {
    | Elaborates(d, ty, delta) =>
      let d' = elab_wrap_builtins(d);
      Elaborates(d', ty, delta);
    | DoesNotElaborate => DoesNotElaborate
    }
  );
};

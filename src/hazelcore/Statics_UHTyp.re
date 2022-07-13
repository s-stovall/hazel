let rec syn_fix_holes:
  (Context.t, IDGen.t, UHTyp.t) => (UHTyp.t, Kind.t, IDGen.t) =
  (ctx, id_gen, ty) =>
    switch (ty) {
    | OpSeq(skel, seq) =>
      let (skel, seq, k, id_gen) =
        syn_fix_holes_skel(ctx, id_gen, skel, seq);
      (OpSeq(skel, seq), k, id_gen);
    }

and syn_fix_holes_skel = (ctx, id_gen, skel, seq) =>
  switch (skel) {
  | Placeholder(n) =>
    let ty_n = seq |> Seq.nth_operand(n);
    let (ty_n, k, id_gen) = syn_fix_holes_operand(ctx, id_gen, ty_n);
    let seq = seq |> Seq.update_nth_operand(n, ty_n);
    (skel, seq, k, id_gen);
  | BinOp(_, op, skel1, skel2) =>
    let (skel1, seq, _, id_gen) =
      ana_fix_holes_skel(ctx, id_gen, skel1, seq, Kind.Type);
    let (skel2, seq, _, id_gen) =
      ana_fix_holes_skel(ctx, id_gen, skel2, seq, Kind.Type);
    switch (Elaborator_Typ.syn_elab(ctx, Delta.empty, UHTyp.mk_OpSeq(seq))) {
    | Some((_, k, _)) =>
      let skel = Skel.BinOp(NotInHole, op, skel1, skel2);
      (skel, seq, k, id_gen);
    | None =>
      failwith("TODO: Add inconsistent kind hole (this can't happen now) 1")
    };
  }

and syn_fix_holes_operand =
    (ctx: Context.t, id_gen: IDGen.t, operand: UHTyp.operand)
    : (UHTyp.operand, Kind.t, IDGen.t) =>
  switch (operand) {
  | Hole => (Hole, Kind.Hole, id_gen)
  | TyVar(err, t) =>
    let next_u = () =>
      switch (err) {
      | NotInTyVarHole => IDGen.next_hole(id_gen)
      | InHole(_, u) => (u, id_gen)
      };
    if (TyVar.reserved_word(t)) {
      let (u, id_gen) = next_u();
      let ty = UHTyp.TyVar(InHole(Reserved, u), t);
      let k = Kind.singleton(HTyp.tyvarhole(Reserved, u, t));
      (ty, k, id_gen);
    } else if (TyVar.valid_name(t)) {
      switch (Context.tyvar_ref(ctx, t)) {
      | None =>
        let (u, id_gen) = next_u();
        let ty = UHTyp.TyVar(InHole(Unbound, u), t);
        let k = Kind.singleton(HTyp.tyvarhole(Unbound, u, t));
        (ty, k, id_gen);
      | Some(cref) =>
        let ty = UHTyp.TyVar(NotInTyVarHole, t);
        let k = Kind.singleton(HTyp.tyvar(ctx, cref.index, t));
        (ty, k, id_gen);
      };
    } else {
      let (u, id_gen) = next_u();
      let ty = UHTyp.TyVar(InHole(InvalidName, u), t);
      (ty, Kind.S(TyVarHole(InvalidName, u, t)), id_gen);
    };
  | Unit => (operand, Kind.singleton(HTyp.product([])), id_gen)
  | Int => (operand, Kind.singleton(HTyp.int()), id_gen)
  | Float => (operand, Kind.singleton(HTyp.float()), id_gen)
  | Bool => (operand, Kind.singleton(HTyp.bool()), id_gen)
  | Parenthesized(body) =>
    let (block, k, id_gen) = syn_fix_holes(ctx, id_gen, body);
    (Parenthesized(block), k, id_gen);
  | List(opseq) =>
    let (opseq, _, id_gen) = syn_fix_holes(ctx, id_gen, opseq);
    switch (Elaborator_Typ.syn_elab(ctx, Delta.empty, opseq)) {
    | Some((ty, _, _)) =>
      let k = Kind.singleton(HTyp.list(ty));
      (List(opseq), k, id_gen);
    | None => failwith(__LOC__ ++ ": impossible branch")
    };
  | Forall(tp, body) =>
    let (ctx, id_gen) =
      switch (tp) {
      | TyVar(_, name) =>
        // TODO: (poly) Discuss InHole?
        (Context.add_tyvar(ctx, name, Kind.Type), id_gen)
      | EmptyHole =>
        // TODO: (poly) consider incrementing id_gen here
        (ctx, id_gen)
      };
    let (body, k, id_gen) = syn_fix_holes(ctx, id_gen, body);
    // FIXME: (poly) kind needs to be updated if k is singleton
    (Forall(tp, body), k, id_gen);
  }

and ana_fix_holes:
  (Context.t, IDGen.t, UHTyp.t, Kind.t) => (UHTyp.t, Kind.t, IDGen.t) =
  (ctx, id_gen, ty, k) =>
    switch (ty) {
    | OpSeq(skel, seq) =>
      let (skel, seq, k', id_gen) =
        ana_fix_holes_skel(ctx, id_gen, skel, seq, k);
      if (Kind.consistent_subkind(ctx, k', k)) {
        (OpSeq(skel, seq), k', id_gen);
      } else {
        failwith(
          "TODO: Add inconsistent kind hole (this can't happen now) 2",
        );
      };
    }

and ana_fix_holes_skel = (tyvars, id_gen, skel, seq, k) =>
  switch (skel) {
  | Placeholder(n) =>
    let ty_n = seq |> Seq.nth_operand(n);
    let (ty_n, k', id_gen) = ana_fix_holes_operand(tyvars, id_gen, ty_n, k);
    let seq = seq |> Seq.update_nth_operand(n, ty_n);
    (skel, seq, k', id_gen);
  | BinOp(_, _, _, _) =>
    let (skel, seq, k', id_gen) =
      syn_fix_holes_skel(tyvars, id_gen, skel, seq);
    if (Kind.consistent_subkind(tyvars, k', k)) {
      (skel, seq, k', id_gen);
    } else {
      failwith("TODO: Add inconsistent kind hole (this can't happen now) 3");
    };
  }

and ana_fix_holes_operand = (ctx, id_gen, operand, k) =>
  switch (operand) {
  | UHTyp.Hole => (Hole, Kind.Hole, id_gen)
  | Parenthesized(body) =>
    let (block, k', id_gen) = ana_fix_holes(ctx, id_gen, body, k);
    if (Kind.consistent_subkind(ctx, k', k)) {
      (Parenthesized(block), k', id_gen);
    } else {
      failwith("TODO: Add inconsistent kind hole (this can't happen now) 4");
    };
  // subsumption
  | TyVar(_)
  | Unit
  | Int
  | Float
  | Bool
  | List(_)
  // TODO: (poly) check rule
  | Forall(_) =>
    let (ty, k', id_gen) = syn_fix_holes_operand(ctx, id_gen, operand);
    if (Kind.consistent_subkind(ctx, k', k)) {
      (ty, k', id_gen);
    } else {
      failwith("TODO: Add inconsistent kind hole (this can't happen now) 5");
    };
  };

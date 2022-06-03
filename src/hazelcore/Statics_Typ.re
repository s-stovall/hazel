let rec syn = (ctx: Context.t, ty: HTyp.t): option(Kind.t) =>
  Log.debug_function(
    __FUNCTION__,
    [("ctx", Context.sexp_of_t(ctx)), ("ty", HTyp.sexp_of_t(ty))],
    ~result_sexp=Sexplib.Std.sexp_of_option(Kind.sexp_of_t),
    () =>
    switch (HTyp.to_syntax(ty)) {
    | Hole
    | TyVarHole(_)
    | Int
    | Float
    | Bool => Some(Kind.singleton(ty))
    | Arrow(ty1, ty2)
    | Sum(ty1, ty2) =>
      open OptUtil.Syntax;
      let* () = ana(ctx, HTyp.of_syntax(ty1), Kind.Type);
      let+ () = ana(ctx, HTyp.of_syntax(ty2), Kind.Type);
      Kind.singleton(ty);
    | Prod(tys) =>
      open OptUtil.Syntax;
      let+ () =
        List.fold_left(
          (opt, ty) =>
            Option.bind(opt, _ => ana(ctx, HTyp.of_syntax(ty), Kind.Type)),
          Some(),
          tys,
        );
      Kind.singleton(ty);
    | List(ty1) =>
      open OptUtil.Syntax;
      let+ _ = ana(ctx, HTyp.of_syntax(ty1), Kind.Type);
      Kind.singleton(ty);
    | TyVar(idx, stamp, _) => Context.tyvar_kind(ctx, idx, stamp)
    }
  )

and ana = (ctx: Context.t, ty: HTyp.t, k: Kind.t): option(unit) =>
  Log.debug_function(
    __FUNCTION__,
    [
      ("ctx", Context.sexp_of_t(ctx)),
      ("ty", HTyp.sexp_of_t(ty)),
      ("k", Kind.sexp_of_t(k)),
    ],
    ~result_sexp=Sexplib.Std.sexp_of_option(Sexplib.Std.sexp_of_unit),
    () =>
    switch (HTyp.to_syntax(ty)) {
    | Hole
    | TyVarHole(_) => Some()
    // subsumption
    | Sum(_)
    | Prod(_)
    | Arrow(_)
    | Int
    | Float
    | Bool
    | List(_)
    | TyVar(_) =>
      open OptUtil.Syntax;
      let* k' = syn(ctx, ty);
      Kind.consistent_subkind(ctx, k', k) ? Some() : None;
    }
  );

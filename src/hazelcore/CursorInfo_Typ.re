type cursor_term = CursorInfo.cursor_term;
type zoperand = CursorInfo_common.zoperand;

let rec extract_cursor_term = (ZOpSeq(_, zseq): ZTyp.t): cursor_term => {
  switch (zseq) {
  | ZOperand(ztyp_operand, _) => extract_from_ztyp_operand(ztyp_operand)
  | ZOperator(ztyp_operator, _) =>
    let (cursor_pos, uop) = ztyp_operator;
    TypOp(cursor_pos, uop);
  };
}
and extract_from_ztyp_operand = (ztyp_operand: ZTyp.zoperand): cursor_term => {
  switch (ztyp_operand) {
  | CursorT(cursor_pos, utyp_operand) => Typ(cursor_pos, utyp_operand)
  | ParenthesizedZ(ztyp)
  | ListZ(ztyp) => extract_cursor_term(ztyp)
  | SumZ(zsumty) => extract_from_zsumtyp(zsumty)
  };
}
and extract_from_zsumtyp = (ZOpSeq(_, zseq): ZTyp.zsumtyp): cursor_term =>
  switch (zseq) {
  | ZOperand(zsumty_operand, _) =>
    extract_from_zsumtyp_operand(zsumty_operand)
  | ZOperator(zsumty_operator, _) =>
    let (cursor_pos, uop) = zsumty_operator;
    SumTypOp(cursor_pos, uop);
  }
and extract_from_zsumtyp_operand =
    (zsumty_operand: ZTyp.zsumtyp_operand): cursor_term =>
  switch (zsumty_operand) {
  | CursorTS(cursor_pos, sumty_operand) => SumTyp(cursor_pos, sumty_operand)
  | ConstTagZ(ztag) => CursorInfo_Tag.extract_cursor_term(ztag)
  | ArgTagZT(ztag, _) => CursorInfo_Tag.extract_cursor_term(ztag)
  | ArgTagZA(_, zty) => extract_cursor_term(zty)
  };

let rec get_zoperand_from_ztyp = (ztyp: ZTyp.t): option(zoperand) => {
  get_zoperand_from_ztyp_opseq(ztyp);
}
and get_zoperand_from_ztyp_opseq = (zopseq: ZTyp.zopseq): option(zoperand) => {
  switch (zopseq) {
  | ZOpSeq(_, zseq) =>
    switch (zseq) {
    | ZOperand(ztyp_operand, _) =>
      get_zoperand_from_ztyp_operand(ztyp_operand)
    | ZOperator(_, _) => None
    }
  };
}
and get_zoperand_from_ztyp_operand =
    (zoperand: ZTyp.zoperand): option(zoperand) => {
  switch (zoperand) {
  | CursorT(_, _) => Some(ZTyp(zoperand))
  | SumZ(zsumty) => Some(ZTyp(SumZ(zsumty)))
  | ParenthesizedZ(ztyp)
  | ListZ(ztyp) => get_zoperand_from_ztyp(ztyp)
  };
};

let cursor_info =
    (~steps as _, ctx: Contexts.t, typ: ZTyp.t): option(CursorInfo.t) =>
  Some(CursorInfo_common.mk(OnType, ctx, extract_cursor_term(typ)));

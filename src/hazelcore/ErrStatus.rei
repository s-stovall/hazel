module HoleReason: {
  /* Variable: `reason` */
  [@deriving (sexp, show)]
  type t =
    | TypeInconsistent
    | WrongLength;

  let eq: (t, t) => bool;
};

/* Variable: `err` */
[@deriving (sexp, show)]
type t =
  | NotInHole
  | InHole(HoleReason.t, MetaVar.t);

/* creates an InHole error status, recycling the metavar from the given err status if available */
let make_recycled_InHole:
  (t, HoleReason.t, MetaVarGen.t) => (t, MetaVarGen.t);

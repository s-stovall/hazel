/**
 * The most recently selected instance for each hole for which a selection
 * has been made.
 */
[@deriving sexp]
type t;

let init: t;
let find_opt: (MetaVar.t, t) => option(HoleClosureId.t);
let add: (MetaVar.t, HoleClosureId.t, t) => t;

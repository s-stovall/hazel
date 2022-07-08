include (module type of Map.Make(String));

module Sexp = Sexplib.Sexp;

[@deriving sexp]
type binding('v) = (string, 'v);

let sexp_of_t: ('v => Sexp.t, t('v)) => Sexp.t;
let t_of_sexp: (Sexp.t => 'v, Sexp.t) => t('v);

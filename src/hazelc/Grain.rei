module Opts: {
  [@deriving sexp]
  type t = {
    grain: option(string),
    includes: option(list(string)),
    optimize: option(int),
    debug: option(bool),
    wat: option(bool),
  };
};

module Compile: {
  [@deriving sexp]
  type opts = {
    file: string,
    output: option(string),
  };

  [@deriving sexp]
  type compile_result = result(unit, int);

  let compile: (~opts: Opts.t, opts) => compile_result;
};

module Run: {
  [@deriving sexp]
  type opts = {wasm: string};

  [@deriving sexp]
  type run_result = result(unit, int);

  let run: (~opts: Opts.t, opts) => run_result;
};

[@deriving sexp]
type opts = Opts.t;

[@deriving sexp]
type compile_opts = Compile.opts;
[@deriving sexp]
type compile_result = Compile.compile_result;
let compile: (~opts: opts, compile_opts) => compile_result;

[@deriving sexp]
type run_opts = Run.opts;
[@deriving sexp]
type run_result = Run.run_result;
let run: (~opts: opts, run_opts) => run_result;

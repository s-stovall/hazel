open Lwt.Infix;
open Sexplib;

/**
   The type of the evaluation result. [None] indicates some error was
   encountered.
 */
type program_result = option(ProgramResult.t);

/**
   The type of the deferred evaluation result. See {!type:program_result}.
 */
type deferred_result = Lwt.t(program_result);

module type M = {
  type t;

  let init: unit => t;

  let get_result: (t, Program.t) => (t, deferred_result);
};

module Sync: M = {
  type t = unit;

  let init = () => ();

  let get_result = (t: t, program: Program.t) => {
    let lwt = Lwt.return(program) >|= Program.get_result >|= Option.some;
    (t, lwt);
  };
};

module Worker = {
  module Inner =
    WebWorker.Make({
      module Request = {
        type t = Program.t;
        type u = string;

        let serialize = program =>
          program |> Program.sexp_of_t |> Sexp.to_string;
        let deserialize = sexp => sexp |> Sexp.of_string |> Program.t_of_sexp;
      };

      module Response = {
        type t = program_result;
        type u = string;

        let serialize = r =>
          r
          |> Sexplib.Conv.sexp_of_option(ProgramResult.sexp_of_t)
          |> Sexp.to_string;
        let deserialize = sexp =>
          sexp
          |> Sexp.of_string
          |> Sexplib.Conv.option_of_sexp(ProgramResult.t_of_sexp);
      };

      module Worker = {
        /* FIXME: Somehow use constant from dune or something? */
        let file = () => "worker.js";

        type state = Sync.t;

        let init_state = Sync.init;

        /* FIXME: Handle exceptions. */
        let on_request = Sync.get_result;
      };
    });

  module Client: M = {
    module Pool = WebWorkerPool.Make(Inner);
    include Pool;

    let init = () => Pool.init(~timeout=2000, ~max=5);

    let get_result = (t: t, program: Program.t) => {
      let res = program |> request(t) >|= Option.join;
      (t, res);
    };
  };

  module Worker = Inner.Worker;
};

open Virtual_dom.Vdom;
open Prompt;

let rank_selection_handler = (x, id) => {
  // update_chosen_rank
  let printing: string = String.concat(" ", [id, x]);
  print_endline(printing);
  Event.Many([]);
};

let a_single_example_expression =
    (example_id: string, example_body: string, ranking_out_of: int) => {
  [
    Node.div(
      [Attr.name("question_wrapper")],
      [
        Node.select(
          [
            Attr.name(example_id),
            Attr.on_change((_, xx) =>
              rank_selection_handler(xx, example_id)
            ),
          ],
          CodeExplanation_common.rank_list(1 + ranking_out_of),
        ),
        Node.text(example_body),
      ],
    ),
  ];
};

// Generate a ranked prompt for each explanation
let render_explanations = (explanations: list(Prompt.explain)): list(Node.t) => {
  List.flatten(
    List.map(
      i => a_single_example_expression(i.id, i.expression, i.rank),
      explanations,
    ),
  );
};

let view = (explanations: list(Prompt.explain)): Node.t => {
  let explanation_view = {
    Node.div(
      [Attr.classes(["the-explanation"])],
      [
        Node.div(
          [Attr.classes(["context-is-empty-msg"])],
          render_explanations(explanations),
        ),
      ],
    );
  };

  Node.div(
    [Attr.classes(["panel", "context-inspector-panel"])],
    [
      Panel.view_of_main_title_bar("Code Explanation"),
      Node.div(
        [Attr.classes(["panel-body", "context-inspector-body"])],
        [explanation_view],
      ),
    ],
  );
};

// this files sets up codemirror related stuff
// exports view vairable so the editor content can be accessed by other files

import { EditorView, basicSetup } from "codemirror";
import { sparql } from "codemirror-lang-sparql";
import { tags } from "@lezer/highlight";
import { HighlightStyle, syntaxHighlighting } from "@codemirror/language";
import { Decoration, DecorationSet, keymap } from "@codemirror/view";
import { StateEffect, StateField } from "@codemirror/state";

import { getColorScheme, notify } from "./util";

const fixedHeightEditor = EditorView.theme(
  {
    "&": { height: "35vh" },
    ".cm-scroller": { overflow: "auto" },
    "&.cm-focused .cm-cursor": { borderLeftColor: "#ddd" },
    "&.cm-focused .cm-selectionBackground, ::selection": {
      backgroundColor: "#353a44",
    },
    ".error-line": { borderBottom: "3px red solid" },
  },
  { dark: getColorScheme() == "dark" },
);

const myHighlightStyle = HighlightStyle.define([
  { tag: tags.keyword, color: "#3584e4" }, // blue
  { tag: tags.variableName, color: "#3a944a" }, // green
  { tag: tags.namespace, color: "#d56199" }, // pink
  { tag: tags.integer, color: "#c88800" }, // yellow
  { tag: tags.float, color: "#c88800" }, // yellow
  { tag: tags.bool, color: "#e62d42" }, // red
  { tag: tags.string, color: "#c88800" }, // yellow
  { tag: tags.comment, color: "#6f8396", fontStyle: "italic" }, // slate grey
  { tag: tags.annotation, color: "#ed5b00" }, // orange
  { tag: tags.url, color: "#2190a4" }, // teal
  { tag: tags.typeName, color: "#ed5b00" }, // orange
]);

const urlParams = new URLSearchParams(window.location.search);
const query = urlParams.get("query"); // queries saved as links will have this param

const runQueryKeymap = keymap.of([
  {
    key: "Shift-Enter",
    preventDefault: true,
    run: () => {
      document.getElementById("run-button").click();
      return true;
    },
  },
]);

// initialise editor
const view = new EditorView({
  doc: query ?? "# Enter your query here\n",
  extensions: [
    basicSetup,
    fixedHeightEditor,
    sparql(),
    syntaxHighlighting(myHighlightStyle),
    runQueryKeymap,
  ],
  parent: document.getElementById("wrapper") ?? undefined,
});

// set up bookmarking functionality
document.getElementById("save-button").addEventListener("click", async () => {
  await navigator.clipboard.writeText(createQueryLink());
  notify("Copied to clipboard");
});

// helper functions

const errorLine = Decoration.mark({
  class: "error-line",
  inclusiveEnd: true,
});
const errorLineEffect = StateEffect.define<{
  start: number;
  end: number | null;
}>();
const errorLineField = StateField.define<DecorationSet>({
  create() {
    return Decoration.none;
  },
  update(field, tr) {
    field = field.map(tr.changes);
    let errorLineTriggered = false;

    for (const e of tr.effects)
      if (e.is(errorLineEffect)) {
        const encoder = new TextEncoder();
        const decoder = new TextDecoder();
        const query = view.state.doc.toString();
        const queryBytesUpToError = encoder
          .encode(query)
          .slice(0, e.value.start + 1);
        const realErrorPos = decoder.decode(queryBytesUpToError).length - 1;
        const charAtErrorLine = query[realErrorPos];

        if (e.value.end) {
          field = field.update({
            add: [errorLine.range(realErrorPos - 1, e.value.end)],
          });
        } else if (charAtErrorLine == "\n") {
          field = field.update({
            add: [errorLine.range(realErrorPos - 1, realErrorPos)],
          });
        } else {
          field = field.update({
            add: [errorLine.range(realErrorPos, realErrorPos + 1)],
          });
        }
        errorLineTriggered = true;
      }

    if (tr.docChanged && !errorLineTriggered) {
      field = field.update({
        filter: (f, t, value) => !value.eq(errorLine),
      });
    }
    return field;
  },
  provide: (f) => EditorView.decorations.from(f),
});

/**
 * Creates red underline where error is detected
 *
 * @param pos - Byte number associated with error position
 */
export function setErrorLine(start: number, end: number | null = null) {
  const effects: StateEffect<unknown>[] = [errorLineEffect.of({ start, end })];

  if (!view.state.field(errorLineField, false))
    effects.push(StateEffect.appendConfig.of([errorLineField]));

  view.dispatch({ effects });
}

/**
 * Generates bookmark link for current content of editor
 *
 * @param query - current content of the editor
 * @returns Bookmark URL
 */
function createQueryLink() {
  const q = encodeURIComponent(String(view.state.doc));
  const currentUrl = new URL(window.location.href);

  return `${currentUrl.origin}?query=${q}`;
}

export default view;

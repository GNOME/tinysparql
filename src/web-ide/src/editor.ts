// this files sets up codemirror related stuff
// exports view vairable so the editor content can be accessed by other files

import {EditorView, basicSetup} from 'codemirror';
import { sparql } from 'codemirror-lang-sparql';
import {tags} from "@lezer/highlight";
import {HighlightStyle, syntaxHighlighting} from "@codemirror/language";
import { Decoration, DecorationSet } from '@codemirror/view';
import { StateEffect, StateField } from '@codemirror/state';

import { createQueryLink, getColorScheme, notify } from './util';

const fixedHeightEditor = EditorView.theme({
    "&": {height: "40vh"},
    ".cm-scroller": {overflow: "auto"},
    "&.cm-focused .cm-cursor": { borderLeftColor: "#ddd" },
    "&.cm-focused .cm-selectionBackground, ::selection": { backgroundColor: "#353a44" },
    ".error-line": { borderBottom : "3px red solid" }
}, { dark: getColorScheme() == "dark" });

const myHighlightStyle = HighlightStyle.define([
  {tag: tags.keyword, color: "#3584e4"}, // blue
  {tag: tags.variableName, color: "#3a944a"}, // green
  {tag: tags.namespace, color: "#d56199"}, // pink
  {tag: tags.integer, color: "#c88800"}, // yellow
  {tag: tags.float, color: "#c88800"}, // yellow
  {tag: tags.bool, color: "#e62d42"}, // red
  {tag: tags.string, color: "#c88800"}, // yellow
  {tag: tags.comment, color: "#6f8396", fontStyle: "italic"}, // slate grey
  {tag: tags.annotation, color: "#ed5b00"}, // orange
  {tag: tags.url, color: "#2190a4"}, // teal
  {tag: tags.typeName, color: "#ed5b00"}, // orange
])

const urlParams = new URLSearchParams(window.location.search);
const query = urlParams.get("query"); // queries saved as links will have this param

// initialise editor
let view = new EditorView({
    doc: query ?? "# Enter your query here\n",
    extensions:  [basicSetup, fixedHeightEditor, sparql(), syntaxHighlighting(myHighlightStyle)],
    parent: document.getElementById("wrapper") ?? undefined
});

// set up bookmarking functionality
document.getElementById("saveBtn").addEventListener("click", async () => {
  await navigator.clipboard.writeText(createQueryLink(view.state.doc));
  notify("Copied to clipboard");
});

const errorLine = Decoration.mark({ 
  class: "error-line",
  inclusiveEnd: true
 });
const errorLineEffect = StateEffect.define<{ pos: number }>();
const errorLineField = StateField.define<DecorationSet>({
  create() {
    return Decoration.none;
  },
  update(field, tr) {
    field = field.map(tr.changes);
    let errorLineTriggered = false;

    for (let e of tr.effects) if (e.is(errorLineEffect)) {
      const encoder = new TextEncoder();
      const decoder = new TextDecoder();
      const query = view.state.doc.toString();
      const queryBytesUpToError = encoder.encode(query).slice(0, e.value.pos + 1);
      const realErrorPos =  decoder.decode(queryBytesUpToError).length - 1;
      const charAtErrorLine = query[realErrorPos];

      if (charAtErrorLine == "\n") {
        field = field.update({
          add: [errorLine.range(realErrorPos - 1, realErrorPos)]
        });
      } else {
        field = field.update({
          add: [errorLine.range(realErrorPos, realErrorPos + 1)]
        });
      }
      errorLineTriggered = true;
    }

    if (tr.docChanged && !errorLineTriggered)
    {
      field = field.update({
        filter: (f, t, value) => !value.eq(errorLine)
      })
    }
    return field;
  },
  provide: f => EditorView.decorations.from(f)
})

export function setErrorLine (pos: number) {
  let effects: StateEffect<unknown>[] = [ errorLineEffect.of({ pos }) ];

  if (!view.state.field(errorLineField, false)) 
    effects.push(StateEffect.appendConfig.of([errorLineField]))
    
  view.dispatch({ effects });
}

export default view;
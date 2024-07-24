// this files sets up codemirror related stuff
// exports view vairable so the editor content can be accessed by other files

import {EditorView, basicSetup} from 'codemirror';
import { sparql } from 'codemirror-lang-sparql';
import {tags} from "@lezer/highlight";
import {HighlightStyle, syntaxHighlighting} from "@codemirror/language";

import { createQueryLink, notify } from './util';

const fixedHeightEditor = EditorView.theme({
    "&": {height: "40vh"},
    ".cm-scroller": {overflow: "auto"},
    "&.cm-focused .cm-cursor": { borderLeftColor: "#ddd" },
    "&.cm-focused .cm-selectionBackground, ::selection": { backgroundColor: "#353a44" },
}, {dark: true});

const myHighlightStyle = HighlightStyle.define([
  {tag: tags.keyword, color: "#B4B4FD"},
  {tag: tags.variableName, color: "#70CFFF"},
  {tag: tags.namespace, color: "#78D454"},
  {tag: tags.integer, color: "#32bd8f"},
  {tag: tags.float, color: "#32bd8f"},
  {tag: tags.bool, color: "#FB7478"},
  {tag: tags.string, color: "#32bd8f"},
  {tag: tags.comment, color: "#7ea2b4", fontStyle: "italic"},
  {tag: tags.annotation, color: "#B4B4FD"},
  {tag: tags.url, color: "#78D454"},
  {tag: tags.typeName, color: "#FB7478"},
  
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

export default view;
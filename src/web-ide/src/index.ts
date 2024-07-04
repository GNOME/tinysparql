import {EditorView, basicSetup} from 'codemirror';
import { sparql } from 'codemirror-lang-sparql';
import run from './run';
import './style.scss';
require('./assets/favicon.ico');

const fixedHeightEditor = EditorView.theme({
    "&": {height: "500px"},
    ".cm-scroller": {overflow: "auto"}
})
  
let view = new EditorView({
    doc: "# Enter your query here\n",
    extensions:  [basicSetup, fixedHeightEditor, sparql()],
    parent: document.getElementById("wrapper") ?? undefined
});

document.getElementById("runBtn")?.addEventListener("click", () => {
   run(String(view.state.doc)); 
});

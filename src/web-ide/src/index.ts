import {EditorView, basicSetup} from 'codemirror';
import { sparql } from 'codemirror-lang-sparql';
import run from './run';
import './style.scss';
import './assets/favicon.ico';

const fixedHeightEditor = EditorView.theme({
    "&": {height: "40vh"},
    ".cm-scroller": {overflow: "auto"}
})
  
let view = new EditorView({
    doc: "# Enter your query here\n",
    extensions:  [basicSetup, fixedHeightEditor, sparql()],
    parent: document.getElementById("wrapper") ?? undefined
});

document.getElementById("runBtn")?.addEventListener("click", async () => {
    let { result, vars } = await run(String(view.state.doc)); 
    document.getElementById("right").replaceChildren(...result);
    if (vars) document.getElementById("variable-selects").replaceChildren(...vars);
    else document.getElementById("variable-selects").innerHTML = "No variables in current query, use this to show or hide variables when there are.";
});

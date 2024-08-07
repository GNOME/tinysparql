import view from './editor';
import { executeSparql } from './run';
import './style.scss';
import './assets/favicon.ico';

document.getElementById("runBtn")?.addEventListener("click", async () => {
    let { result, vars } = await executeSparql(String(view.state.doc), "http://127.0.0.1:1234/sparql"); // address of separate backend server in development mode

    // fill results section with error/results table etc.
    document.getElementById("right").replaceChildren(...result);

    // update varaible checkbox area to allow show/hide of variables in results table
    if (vars) document.getElementById("variable-selects").replaceChildren(...vars);
    else document.getElementById("variable-selects").innerHTML = "No variables in current query, use this to show or hide variables when there are.";
});

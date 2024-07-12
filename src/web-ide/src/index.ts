import view from './editor';
import run from './run';
import './style.scss';
import './assets/favicon.ico';

document.getElementById("runBtn")?.addEventListener("click", async () => {
    let { result, vars } = await run(String(view.state.doc), "http://127.0.0.1:1234/sparql"); //todo: make this more flexible
    document.getElementById("right").replaceChildren(...result);
    if (vars) document.getElementById("variable-selects").replaceChildren(...vars);
    else document.getElementById("variable-selects").innerHTML = "No variables in current query, use this to show or hide variables when there are.";
});

// contains all initialisation and main view update functions

import view from './editor';
import { executeQuery, getPrefixes } from './xhr';
import { setErrorLine } from "./editor";
import { SparqlData } from './xhr';

import './style.scss';
import './assets/favicon.ico';
import { generateCheckbox, setLoading } from './util';

// initialise ontology prefix mapping
let prefixes: Record<string, string> = {};
window.addEventListener("load", async () => {
    prefixes = await getPrefixes();
});

// main execution lifecycle
document.getElementById("runBtn")?.addEventListener("click", async () => {
   
    setLoading(document.querySelector("#right>div"));
    setLoading(document.getElementById("variable-selects"))
    const execRes = await executeQuery(String(view.state.doc));

    let resultsDisplayElements:HTMLElement[] = [];
    if (execRes.kind == "data") {
        const vars = generateVarSelects(execRes.data);
        // update varaible checkbox area to allow show/hide of variables in results table
        resultsDisplayElements.push(generateResultsTable(execRes.data));
        document.getElementById("variable-selects").replaceChildren(...vars);
    } else {
        const msg = execRes.kind == "error" ? processError(execRes.error) : execRes.error;
        resultsDisplayElements.push(generateErrorMessage(msg))
        document.getElementById("variable-selects").innerHTML = "No variables in current query, use this to show or hide variables when there are.";
    }

    // fill results section with error/results table etc.
    document.querySelector("#right>div").replaceChildren(...resultsDisplayElements);
});






// helper functions 

/**
 * Creates DOM elements relevant to producing the full results table
 * 
 * Uses ontology prefixes to shorten url values
 *
 * @param data - Parsed JSON result object from sparql endpoint
 * @param show - List of variables to show in result table
 * @returns Div element containing formatted results table
 */
function generateResultsTable(data: SparqlData, show: string[]|null = null, showRowNum: boolean = true):HTMLDivElement {
    const tablediv = document.createElement("div");
    tablediv.classList.add("table-container");

    const table = document.createElement("table");
    const thead = table.createTHead();
    const tbody = table.createTBody();
    thead.insertRow(0);
    table.classList.add("table", "is-striped", "is-hoverable");

    const vars = show ?? data.head.vars;
    if (showRowNum) {
        let thvar = document.createElement("th");
        thvar.innerText = "row";
        thead.appendChild(thvar);
    } 
    vars.forEach(v => {
        let th = document.createElement("th");
        th.innerText = v;
        thead.appendChild(th);
    });

    const bindings = data.results.bindings;
    bindings.forEach((b, i) => {
        let row = tbody.insertRow(i);
        let offset = 0;
        if (showRowNum) {
            let id = row.insertCell(0);
            id.innerText = String(i + 1);
            offset = 1;
        }
        vars.forEach((v, j) => {
            let cell = row.insertCell(j + offset);
            const value = b[v].value;
            
            // if value is a url, shorten with a prefix if one exists, otherwise use value as is
            const urlMatch = value.match(/^(https?:\/\/[A-Za-z.\/0-9-_]+#?)[a-zA-z]*$/);
            if (urlMatch) {
                const url = urlMatch[1];
                const prefix = prefixes[url];
                const valueWithPrefix = prefix ? value.replace(url, `${ prefix }: `) : value;
                cell.innerHTML = 
                // links to relevant docs
                `<a href=${ urlMatch[0] } target="_blank" rel="noopener noreferrer">
                    ${ valueWithPrefix }
                </a>`;
            }
            else cell.innerText = value;
        })
    })

    tablediv.appendChild(table);
    return tablediv;
}

/**
 * Generates DOM elements for displaying query errors
 *
 * @param error - The error message returned by the SPARQL request
 * @returns Div element containing formatted error message
 */
function generateErrorMessage(error:string): HTMLDivElement {
    const div = document.createElement("div");
    div.classList.add("is-flex", "is-flex-direction-column", "is-flex-grow-1", "is-align-items-start", "is-justify-content-center", "is-background-danger-dark", "px-6", "py-4");

    const title = document.createElement("p");
    title.innerHTML = "<strong>Query failed!</strong>";

    const p = document.createElement("p");
    p.innerText = error;

    div.appendChild(title);
    div.appendChild(p);
    return div;
}

/**
 * Generates checkboxes used for hiding/showing variables in the results table
 *
 * @param error - The error message returned by the SPARQL request
 * @returns List of elements to be added to variable toggle space
 */
function generateVarSelects(data:SparqlData):HTMLElement[] {
    // default behaviour when no variables
    if (!data.head.vars) {
        const s = document.createElement("span");
        s.innerText = "No variables in current query, use this to show or hide variables when there are.";
        return [s];
    }

    const varCheckboxes = data.head.vars.map(v =>
        generateCheckbox(v, ["var-checkbox"], changeDisplayVars, data)
    );

    const rowNumToggle = generateCheckbox("row number", ["rownum-checkbox"], changeDisplayVars, data);

    return [rowNumToggle, ...varCheckboxes];
}

/**
 * Toggle function to show/hide specific variable columns in the results table
 * 
 * Calls generateResultsTable with current checked variables as "show" argument
 *
 * @param data - Parsed JSON result object from sparql endpoint
 */
function changeDisplayVars(data:SparqlData) {
    setLoading(document.querySelector("#right>div"));
    const checkboxes = document.getElementById("variable-selects").querySelectorAll("input");
    const showVars = Array.from(checkboxes)
        .filter(c => c.checked && c.classList.contains("var-checkbox"))
        .map(c => c.parentNode.querySelector("span").innerText );
        
    const showRowNum: HTMLInputElement = document.getElementById("variable-selects").querySelector("input.rownum-checkbox");
    const newTable = generateResultsTable(data, showVars, showRowNum.checked);
    setTimeout(() => {
        document.querySelector("#right>div").replaceChildren(newTable);
    }, 50);
}

/**
 * Generates the correct error message from information in the HTTP response
 * 
 * Calls setErrorLine to trigger error highlighting in editor in case of parsing errors
 *
 * @param errorRes - HTTP Response from SPARQL endpoint reporting an error
 * @returns Relevant error message
 */
function processError(errorRes:Response): string {
    let m = "Something went wrong! Please try again."; // default error text
    if (errorRes.status == 400) {
        m = errorRes.statusText;
        const parseErr = /Parser error at byte [0-9]+/g.exec(m);
        if (parseErr !== null) {
            const pos = parseErr[0].split(" ")[4];
            setErrorLine(Number(pos));
        }
    }
    else if (errorRes.status == 500) m = "Internal server error!";
    return m;
  }
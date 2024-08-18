// contains all initialisation and main view update functions

import view from "./editor";
import { executeQuery, getPrefixes } from "./xhr";
import { setErrorLine } from "./editor";
import { SparqlData } from "./xhr";

import "./style.scss";
import "./assets/favicon.ico";
import { generateCheckbox, setLoading } from "./util";

// initialise ontology prefix mapping
let prefixes: Record<string, string> = {};
window.addEventListener("load", async () => {
  prefixes = await getPrefixes();
});

// main execution lifecycle
document.getElementById("run-button")?.addEventListener("click", async () => {
  setLoading(document.querySelector("#results-pane>div"));
  setLoading(document.getElementById("variable-selects"));
  const execRes = await executeQuery(String(view.state.doc));

  const resultsDisplayElements: HTMLElement[] = [];
  if (execRes.kind == "data") {
    const vars = generateVarSelects(execRes.data);
    // update varaible checkbox area to allow show/hide of variables in results table
    resultsDisplayElements.push(generateResultsTable(execRes.data));
    document.getElementById("variable-selects").replaceChildren(...vars);
  } else {
    const msg =
      typeof execRes.error == "string"
        ? execRes.error
        : processError(execRes.error);
    resultsDisplayElements.push(generateErrorMessage(msg));
    document.getElementById("variable-selects").innerHTML =
      "No variables in current query, use this to show or hide variables when there are.";
  }

  // fill results section with error/results table etc.
  document
    .querySelector("#results-pane>div")
    .replaceChildren(...resultsDisplayElements);
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
function generateResultsTable(data: SparqlData): HTMLDivElement {
  const tablediv = document.createElement("div");
  tablediv.classList.add("table-container");

  const table = document.createElement("table");
  const thead = table.createTHead();
  const tbody = table.createTBody();
  thead.insertRow(0);
  table.classList.add("table", "is-striped", "is-hoverable");

  const vars = data.head.vars;

  const thvar = document.createElement("th");
  thvar.innerText = "row";
  thvar.classList.add("row-num");
  thead.appendChild(thvar);
  vars.forEach((v) => {
    const th = document.createElement("th");
    th.innerText = v;
    th.classList.add(`${v}-column`);
    thead.appendChild(th);
  });

  const bindings = data.results.bindings;
  bindings.forEach((b, i) => {
    const row = tbody.insertRow(i);
    let offset = 0;

    const id = row.insertCell(0);
    id.innerText = String(i + 1);
    id.classList.add("row-num");
    offset = 1;

    vars.forEach((v, j) => {
      const cell = row.insertCell(j + offset);
      cell.classList.add(`${v}-column`);
      const value = b[v].value;

      // if value is a url, shorten with a prefix if one exists, otherwise use value as is
      const urlMatch = value.match(
        /^(https?:\/\/[A-Za-z./0-9-_%]+#?)[a-zA-z]*$/,
      );
      const fileUriMatch = value.match(/^file:\/\/[A-Za-z./0-9-_%]+$/);
      if (urlMatch) {
        const url = urlMatch[1];
        const prefix = prefixes[url];
        const valueWithPrefix = prefix
          ? value.replace(url, `${prefix}:`)
          : value;
        cell.innerHTML =
          // links to relevant docs
          `<a href="${urlMatch[0]}" target="_blank" rel="noopener noreferrer">
                    ${valueWithPrefix}
                </a>`;
      } else if (fileUriMatch) {
        cell.innerHTML = `<a href="${value}" target="_blank" rel="noopener noreferrer">
                    ${value} 
                </a>`;
      } else cell.innerText = value;
    });
  });

  tablediv.appendChild(table);
  return tablediv;
}

/**
 * Generates DOM elements for displaying query errors
 *
 * @param error - The error message returned by the SPARQL request
 * @returns Div element containing formatted error message
 */
function generateErrorMessage(error: string): HTMLDivElement {
  const div = document.createElement("div");
  div.classList.add(
    "is-flex",
    "is-flex-direction-column",
    "is-flex-grow-1",
    "is-align-items-start",
    "is-justify-content-center",
    "is-background-danger-soft",
    "px-6",
    "py-4",
  );

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
function generateVarSelects(data: SparqlData): HTMLElement[] {
  // default behaviour when no variables
  if (!data.head.vars) {
    const s = document.createElement("span");
    s.innerText =
      "No variables in current query, use this to show or hide variables when there are.";
    return [s];
  }

  const varCheckboxes = data.head.vars.map((v) =>
    generateCheckbox(v, ["var-checkbox"], (c) => {
      changeDisplayVars(c, `${v}-column`);
    }),
  );

  const rowNumToggle = generateCheckbox(
    "row number",
    ["rownum-checkbox"],
    (c) => {
      changeDisplayVars(c, `row-num`);
    },
  );

  return [rowNumToggle, ...varCheckboxes];
}

/**
 * Toggle function to show/hide specific variable columns in the results table
 *
 * Calls generateResultsTable with current checked variables as "show" argument
 *
 * @param show - whether to show or hide the column
 * @param className - class name of the column to toggle
 */
function changeDisplayVars(show: boolean, className: string) {
  document.querySelectorAll(`.${className}`).forEach((e) => {
    if (!show) e.setAttribute("hidden", "true");
    else e.removeAttribute("hidden");
  });
}

/**
 * Generates the correct error message from information in the HTTP response
 *
 * Calls setErrorLine to trigger error highlighting in editor in case of parsing errors
 *
 * @param errorRes - HTTP Response from SPARQL endpoint reporting an error
 * @returns Relevant error message
 */
function processError(errorRes: Response): string {
  let m: string;
  if (errorRes.status == 400) {
    m = errorRes.statusText;
    const parseErr = /Parser error at byte [0-9]+/g.exec(m);
    if (parseErr !== null) {
      const pos = parseErr[0].split(" ")[4];
      setErrorLine(Number(pos));
    }
  } else if (errorRes.status == 404) {
    m = "SPARQL endpoint not found";
  } else if (errorRes.status == 500) {
    m = "Internal server error!";
  } else {
    m = errorRes.statusText;
  }

  return m;
}

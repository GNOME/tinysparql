export default async function run(s: string) {
    let reqHead: Headers = new Headers({
        "Content-Type": "text/plain",
        "Accept": "application/sparql-results+json"
    });

    let reqOptions: RequestInit = {
        mode: 'cors',
        method: 'POST',
        headers: reqHead,
        body: s,
        redirect: 'follow'
    };

    let resultHTML: HTMLElement[];
    try {
        let res = await fetch("http://127.0.0.1:1234/sparql/", reqOptions);
        let parsedResults = JSON.parse(await res.text());
        
        resultHTML = [generateResultsTable(parsedResults)]
    } catch (error) {
        console.error(error);
        if (typeof error == 'string') {
            resultHTML = [generateErrorMessage(error)];
        } else if (error instanceof Error) {
            resultHTML = [generateErrorMessage(error.message)];
        }
    }

    document.querySelector("#right").replaceChildren(...resultHTML);
}

type sparqlRes = {
    head: {
        vars: string[]
    },
    results: {
        bindings: Record<string, { type: string, value: string }>[]
    }
}

function generateResultsTable(data: sparqlRes) {
    const tablediv = document.createElement("div");
    tablediv.classList.add("table-container");

    const table = document.createElement("table");
    const thead = table.createTHead();
    const tbody = table.createTBody();
    thead.insertRow(0);
    table.classList.add("table")

    const vars = data.head.vars;
    vars.forEach(v => {
        let th = document.createElement("th");
        th.innerHTML = v;
        thead.appendChild(th);
    });

    const bindings = data.results.bindings;
    bindings.forEach((b, i) => {
        let row = tbody.insertRow(i);
        vars.forEach((v, j) => {
            let cell = row.insertCell(j);
            cell.innerHTML = b[v].value;
        })
    })

    tablediv.appendChild(table);
    return tablediv;
}

function generateErrorMessage(error:string) {
    const div = document.createElement("div");
    div.classList.add("is-flex", "is-flex-direction-column", "is-flex-grow-1", "is-align-items-center", "is-justify-content-center", "is-background-warning-dark");

    const title = document.createElement("p");
    title.innerText = "Query failed!";
    title.classList.add("has-text-weight-bold");

    const p = document.createElement("p");
    p.innerText = error;
    p.classList.add("subtitle");

    div.appendChild(title);
    div.appendChild(p);
    return div;
}
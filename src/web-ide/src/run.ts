type runRes = {
    result: HTMLElement[],
    vars?: HTMLElement[]
};

export default async function run(s: string):Promise<runRes> {
    if(!s) {
        return {
            result :[generateErrorMessage("Empty query. Enter your query into the editor space and try again.")]
        };
    }

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

    
    try {
        let res = await fetch("http://127.0.0.1:1234/sparql/", reqOptions);
        if (res.ok) {
            let parsedResults = JSON.parse(await res.text());
            return {
                result: [generateResultsTable(parsedResults)],
                vars: generateVarSelects(parsedResults)
            };
        }

        let m = "Something went wrong! Please try again."; // default error text
        if (res.status == 400) m = res.statusText;
        else if (res.status == 500) "Internal server error!";

        return {
            result: [generateErrorMessage(m)]
        };
       
    } catch (error) {
        console.error(error);
        let m;
        if (typeof error == 'string') {
           m = error;
        } else if (error instanceof Error) {
           m = error.message;
        }

        return {
            result: [generateErrorMessage(m)]
        };
    }
}

type sparqlRes = {
    head: {
        vars: string[]
    },
    results: {
        bindings: Record<string, { type: string, value: string }>[]
    }
}

function generateResultsTable(data: sparqlRes, show: string[]|null = null) {
    const tablediv = document.createElement("div");
    tablediv.classList.add("table-container");

    const table = document.createElement("table");
    const thead = table.createTHead();
    const tbody = table.createTBody();
    thead.insertRow(0);
    table.classList.add("table", "is-striped", "is-hoverable");

    const vars = show ?? data.head.vars;
    let thvar = document.createElement("th");
    thvar.innerText = "row";
    thead.appendChild(thvar);
    vars.forEach(v => {
        let th = document.createElement("th");
        th.innerText = v;
        thead.appendChild(th);
    });

    const bindings = data.results.bindings;
    bindings.forEach((b, i) => {
        let row = tbody.insertRow(i);
        let id = row.insertCell(0);
        id.innerText = String(i + 1);
        vars.forEach((v, j) => {
            let cell = row.insertCell(j + 1);
            cell.innerText = b[v].value;
        })
    })

    tablediv.appendChild(table);
    return tablediv;
}

function generateErrorMessage(error:string) {
    const div = document.createElement("div");
    div.classList.add("is-flex", "is-flex-direction-column", "is-flex-grow-1", "is-align-items-start", "is-justify-content-center", "is-background-danger-dark", "px-6", "py-4");

    const title = document.createElement("p");
    title.innerText = "Query failed!";
    title.classList.add("has-text-weight-bold");

    const p = document.createElement("p");
    p.innerText = error;

    div.appendChild(title);
    div.appendChild(p);
    return div;
}

function generateVarSelects(data:sparqlRes) {
    if (!data.head.vars) {
        const s = document.createElement("span");
        s.innerText = "No variables in current query, use this to show or hide variables when there are.";
        return [s];
    }

    return data.head.vars.map(v => {
        const label = document.createElement("label");
        const checkbox = document.createElement("input");
        const text = document.createElement("span");

        label.classList.add("checkbox");
        checkbox.setAttribute("type", "checkbox");
        checkbox.setAttribute("checked", "true");
        text.innerText = v;
        text.classList.add("ml-2");
        label.appendChild(checkbox);
        label.appendChild(text);

        checkbox.addEventListener("change", () => {
            changeDisplayVars(data);
        });
        return label;
    });
}

function changeDisplayVars(data:sparqlRes) {
    const checked = document.getElementById("variable-selects").querySelectorAll("input");
    const showVars = Array.from(checked)
        .filter(c => c.checked)
        .map(c => c.parentNode.querySelector("span").innerText );
    document.getElementById("right").replaceChildren(generateResultsTable(data, showVars));
}
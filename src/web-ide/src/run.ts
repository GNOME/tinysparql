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

    try {
        let res = await fetch("/sparql/", reqOptions);
        let parsedResults = JSON.parse(await res.text());
    
        fillTable(parsedResults);
    } catch (error) {
        console.error(error)
        document.getElementById("error").innerHTML = error; //todo: better error reporting mechanism
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

function fillTable(data: sparqlRes){
    let table = document.getElementById("table");

    if (!(table instanceof HTMLTableElement)) {
        document.getElementById("error").innerHTML = "Table element not found.";
        return;
    }

    table.innerHTML = "";
    let HeaderRow = table.createTHead().insertRow(0);
    const vars = data.head.vars;

    for (let i = 0; i < vars.length; i++){
        let th = document.createElement("th");
        th.innerHTML = vars[i];
        HeaderRow.appendChild(th);
    }

    const bindings = data.results.bindings;
    for (let i = 0; i < bindings.length; i++){
        let row = table.insertRow(i+1);
        for (let j = 0; j < vars.length; j++){
            let cell = row.insertCell(j);
            cell.innerHTML = bindings[i][vars[j]].value;
        }
    }
}
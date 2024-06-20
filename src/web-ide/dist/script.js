function send() {
    var myHeaders = new Headers();
    myHeaders.append("Content-Type", "text/plain");
    myHeaders.append("Accept", "application/sparql-results+json");

    console.log("here in send")
    console.log(document.getElementById("textInput").value)

    // var raw = "Ask {?u a rdfs:Resource}";
    var inputText = document.getElementById("textInput").value;

    var requestOptions = {
        mode: 'cors',
        method: 'POST',
        headers: myHeaders,
        body: inputText,
        redirect: 'follow'
    };

    fetch("/sparql/", requestOptions)
        .then(response => response.text())
        .then(result => {
            console.log(JSON.parse(result))
            fillTable(JSON.parse(result))
        })
        .catch(error => {
            console.log('error', error)
            divError = document.getElementById("error");
            divError.innerHTML = error;
        });
}

function fillTable(data){
    var table = document.getElementById("table");
    table.innerHTML = "";
    var HeaderRow = table.createTHead().insertRow(0);
    const vars = data.head.vars;
    for (var i = 0; i < vars.length; i++){
        let th = document.createElement("th");
        th.innerHTML = vars[i];
        HeaderRow.appendChild(th);
    }

    const bindings = data.results.bindings;
    for (var i = 0; i < bindings.length; i++){
        var row = table.insertRow(i+1);
        for (var j = 0; j < vars.length; j++){
            let cell = row.insertCell(j);
            cell.innerHTML = bindings[i][vars[j]].value;
        }
    }
}
import view from './editor';
import { executeSparql, getEndpointDescriptor } from './run';
import './style.scss';
import './assets/favicon.ico';

// For now the web IDE runs on the same port as the HTTP endpoint.
// This will change once <https://gitlab.gnome.org/GNOME/tinysparql/-/issues/450>
// is implemented.
const endpointUrl = new URL("/sparql", window.location.href);

document.getElementById("runBtn")?.addEventListener("click", async () => {
    let { result, vars } = await executeSparql(String(view.state.doc), endpointUrl.toString());

    // fill results section with error/results table etc.
    document.getElementById("right").replaceChildren(...result);

    // update varaible checkbox area to allow show/hide of variables in results table
    if (vars) document.getElementById("variable-selects").replaceChildren(...vars);
    else document.getElementById("variable-selects").innerHTML = "No variables in current query, use this to show or hide variables when there are.";
});

window.onload = async (event) => {
    console.log("Connecting to endpoint to fetch service description.");

    let connectionStatus = null;
    try {
        let descriptor = await getEndpointDescriptor(endpointUrl.toString());

        let endpointName = descriptor["@graph"][0]["dc:title"];
        document.getElementById("endpoint-name").innerHTML = endpointName;

        connectionStatus = `Connected to endpoint: ${endpointUrl}`;
    } catch (error) {
        connectionStatus = `Failed to connect to ${endpointUrl}: ${error}`;
    }

    document.getElementById("connection-status").innerHTML = connectionStatus;
}

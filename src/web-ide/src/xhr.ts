// this file contains all functions related to XHR requests

// comment line below when using dev server
//import { ENDPOINT_URL as endpoint } from './const.prod';
// uncomment line below to use dev server
import { ENDPOINT_URL as endpoint } from './const';

export interface SparqlData {
    head: {
        vars: string[]
    },
    results: {
        bindings: Record<string, { type: string, value: string }>[]
    }
}

interface DataRes {
    kind: "data",
    data: SparqlData
}

interface ErrorRes {
    kind: "error",
    error: Response | string
}

type SparqlRes = DataRes | ErrorRes;

/**
 * Fetches prefix descriptions from current endpoint
 *
 * @returns {Promise<Record<string,string>>} Mapping of url to prefix.
 */
export async function getPrefixes(): Promise<Record<string,string>> {
    let prefixes: Record<string, string> = {}; // maps url to prefix
    
    // using turtle here because of issue #468, JSON-LD would be more intuitive otherwise
    let reqHead: Headers = new Headers({
        "Content-Type": "text/plain",
        "Accept": "text/turtle"
    });
    
    let reqOptions: RequestInit = {
        mode: 'cors',
        method: 'POST',
        headers: reqHead,
        body: "DESCRIBE <>",
        redirect: 'follow'
    };
    
    
    let res = await fetch(endpoint, reqOptions);
    const raw = await res.text();

    // extract every line describing a prefix
    Array.from(raw.matchAll(/@prefix ([a-z]+): <(https?:\/\/[A-Za-z.\/0-9-_]+#?)>/g))
        .forEach(m => {
            const prefix = m[1];
            const url = m[2];
            prefixes[url] = prefix;
        });
    
    return prefixes;
}

/**
 * Execute a SPARQL query on the given the HTTP endpoint.
 *
 * Results are returned to the caller via a SparqlRres instance.
 *
 * This will provide the caller with either one of the following:
 * 
 * 1. Parsed JSON result object from sparql endpoint
 * 
 * 2. Response object containing error to be processed later on
 * 
 * 3. Error message from any other errors raised (not from response)
 *
 * @param query - The SPARQL query to execute.
 * @returns Result object promise.
 * @throws Connection Timeout error
 */

export async function executeQuery(query:string): Promise<SparqlRes> {
    try {
        if(!query) {
            throw "Empty query. Enter your query into the editor space and try again.";
        }

        let reqHead: Headers = new Headers({
            "Content-Type": "text/plain",
            "Accept": "application/sparql-results+json"
        });

        let reqOptions: RequestInit = {
            mode: 'cors',
            method: 'POST',
            headers: reqHead,
            body: query,
            redirect: 'follow'
        };

        const res = await new Promise<Response>(async (resolve, reject) => {
            const connTimeout = setTimeout(() => {
                reject("Connection Timeout: Cannot reach SPARQL endpoint");
            }, 3000)
            const r = await fetch(endpoint, reqOptions);
            clearTimeout(connTimeout);
            resolve(r);
        });
        
        if (res.ok) {
            let parsedResults = JSON.parse(await res.text());
            return {
                kind: "data",
                data: parsedResults
            }
        } else return {
            kind: "error",
            error: res
        }
    } catch (error) {
        console.error(error);

        let m;
        if (typeof error == "string") {
            m = error;
        } else if (error instanceof Error) {
            m = error.message;
        }

        return {
            kind: "error",
            error: m
        }
    }
}


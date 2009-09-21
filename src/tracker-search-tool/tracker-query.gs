[indent=4]

[DBus (name = "org.freedesktop.Tracker1.Resources")]
interface Resources : GLib.Object 
    def abstract SparqlQuery (query : string) : array of string[,]  raises DBus.Error
    def abstract SparqlUpdate (query : string) raises DBus.Error


class TrackerQuery : Object

    tracker : Resources;

    event SearchSettingsChanged ()   
    event ClearSearchResults () 

    prop SearchTerms : string
    prop Category : string
    prop SortField : string
    prop Fields : array of string
    
    
    init
        Category = "All"
    
         self.notify += def (t, propety)
            if propety.name is "Category" or  propety.name is "SortField" or propety.name is "Fields"
                SearchSettingsChanged ()
            else 
                if propety.name is "SearchTerms" 
                    if SearchTerms is null or SearchTerms.length < 3    
                        ClearSearchResults ()
                    else 
                        SearchSettingsChanged ()
                        

    def Connect () : bool
        
        try    
            var conn = DBus.Bus.get (DBus.BusType.SESSION)
            tracker = conn.get_object ("org.freedesktop.Tracker1", "/org/freedesktop/Tracker1/Resources", "org.freedesktop.Tracker1.Resources") as Resources
        except e : DBus.Error
            print "Cannot connect to Session bus. Error is %s", e.message     
            return false
            
        return true
       
       
    def Search () : array of string[,] 
    
        query : string
    
        if Category is null or Category is "All"    
            query = "SELECT ?s WHERE { ?s fts:match \"%s\". ?s a nie:InformationElement } limit 100 ".printf(SearchTerms)
        else
            query = "SELECT ?s WHERE { ?s fts:match \"%s\". ?s a %s} limit 100 ".printf(SearchTerms, Category)   

        // to do : add Fields, Category and SortField
        return tracker.SparqlQuery (query)
        
        
    def Query (sparql : string) : array of string[,] 
        return tracker.SparqlQuery (sparql)
        
        
        
  
        
            

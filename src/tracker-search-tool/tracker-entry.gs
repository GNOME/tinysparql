[indent=4]

uses
    Gtk
    
    
class TrackerSearchEntry  : Gtk.Entry

    prop Query : TrackerQuery 

    init
        set_icon_from_stock (EntryIconPosition.PRIMARY, STOCK_FIND)
        set_icon_from_stock (EntryIconPosition.SECONDARY, STOCK_CLEAR)
        set_icon_sensitive (EntryIconPosition.SECONDARY, false)
        set_icon_tooltip_text (EntryIconPosition.SECONDARY, _("Clear the search text"))
        changed += entry_changed
        
        
    def private entry_changed (editable : Editable) 
        if Query is not null
            if text is null
                Query.SearchTerms = ""
            else
                Query.SearchTerms = text
        
    
    


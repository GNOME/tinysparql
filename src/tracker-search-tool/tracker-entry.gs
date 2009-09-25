[indent=4]

uses
    Gtk
    TrackerUtils
    
    
class TrackerSearchEntry  : Gtk.Entry
    prop Query : TrackerQuery 

    init
        set_icon_from_stock (EntryIconPosition.PRIMARY, STOCK_FIND)
        set_icon_from_stock (EntryIconPosition.SECONDARY, STOCK_CLEAR)
        set_icon_sensitive (EntryIconPosition.PRIMARY, false)
        set_icon_sensitive (EntryIconPosition.SECONDARY, false)
        set_icon_tooltip_text (EntryIconPosition.SECONDARY, _("Clear the search text"))
        changed += entry_changed
        icon_press += def (p0, p1)
            if p0 is EntryIconPosition.SECONDARY
                text = "" 
        
        
    def private entry_changed (editable : Editable) 
        if Query is not null
            if text is null
                set_icon_sensitive (EntryIconPosition.SECONDARY, false)
                Query.SearchTerms = ""
            else
                set_icon_sensitive (EntryIconPosition.SECONDARY, true)
                Query.SearchTerms = EscapeSparql (text)
        
    
    


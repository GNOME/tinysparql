CREATE TRIGGER delete_service BEFORE DELETE ON Services 
BEGIN  
	DELETE FROM ServiceMetaData WHERE ServiceID = old.ID;
	DELETE FROM ServiceKeywordMetaData WHERE ServiceID = old.ID;
	DELETE FROM ServiceNumericMetaData WHERE ServiceID = old.ID;
	DELETE FROM ChildServices WHERE (ParentID = old.ID);
	DELETE FROM ChildServices WHERE (ChildID = old.ID);
	
END;!

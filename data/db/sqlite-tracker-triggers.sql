CREATE TRIGGER delete_backup_service BEFORE DELETE ON BackupServices 
BEGIN  
	DELETE FROM BackupMetaData WHERE ServiceID = old.ID;
END;!

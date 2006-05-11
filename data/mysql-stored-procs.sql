
-- general purpose functions --

DROP FUNCTION if exists GetServiceTypeID;|

CREATE FUNCTION GetServiceTypeID (pService varchar (32))
RETURNS int
BEGIN
	Declare result int;

	IF (pService = 'FileOther') THEN
		Set result = 0;
	ELSEIF (pService = 'VFSOther') THEN
		Set result = 5;
	ELSE
		select TypeID into result From ServiceTypes where TypeName = pService;
	END IF;

	return result;
END|


/* where a service has a range of values, this function returns the uppermost ServiceTypeID */
DROP FUNCTION if exists GetMaxServiceTypeID;|

CREATE FUNCTION GetMaxServiceTypeID (pService varchar (32))
RETURNS int
BEGIN
	Declare result int;

	IF (pService = 'Files') THEN
		Set result = 4;
	ELSEIF (pService = 'VFSFiles') THEN
		Set result = 9;
	ELSEIF (pService = 'FileOther') THEN
		Set result = 0;
	ELSEIF (pService = 'VFSOther') THEN
		Set result = 5;
	ELSE
		Set result = GetServiceTypeID (pService);		
	END IF;

	return result;
END|


-- service SPs --

DROP PROCEDURE if exists GetServiceID;|

CREATE PROCEDURE GetServiceID (pPath varchar(200), pName varchar(128)) 
BEGIN 
	SELECT ID, IndexTime, IsDirectory FROM Services WHERE Path = pPath AND Name = pName;
END|


DROP PROCEDURE if exists CreateService;|

CREATE PROCEDURE CreateService (pPath varchar(200), pName varchar(128), pServiceType varchar(32), pIsDirectory bool, pIsLink bool, pIsServiceSource bool, pOffset int unsigned, pIndexTime int unsigned) 
BEGIN 

	Declare pID int unsigned;
	Declare pServiceTypeID int;

	SELECT TypeID INTO pServiceTypeID From ServiceTypes where TypeName = pServiceType;

	UPDATE sequence SET id=LAST_INSERT_ID(id+1);
	SELECT LAST_INSERT_ID() into pID;

	IF (pServiceType != 'Emails') THEN
		INSERT INTO Services (ID, Path, Name, ServiceTypeID, IsDirectory, IsLink,  IsServiceSource, Offset, IndexTime) VALUES (pID, pPath,pName,pServiceTypeID,pIsDirectory,pIsLink, pIsServiceSource, pOffset, pIndexTime); 
	END IF;
END|


DROP PROCEDURE if exists DeleteService;|

CREATE PROCEDURE DeleteService (pID int unsigned)
BEGIN 
	
	DELETE FROM Services WHERE ID = pID;
	DELETE FROM ServiceMetaData WHERE ServiceID = pID;
	DELETE FROM ServiceLinks WHERE (ServiceID = pID or LinkID = pID);
	
END|


DROP PROCEDURE if exists DeleteEmbeddedServiceMetadata;|

CREATE PROCEDURE DeleteEmbeddedServiceMetadata (pID int unsigned)
BEGIN 

	DELETE FROM ServiceMetaData WHERE ServiceID = pID AND MetaDataID IN (SELECT ID FROM MetaDataTypes WHERE Embedded = 1);
	
END|


-- File SPs --

DROP PROCEDURE if exists SelectFileChild;|

CREATE PROCEDURE SelectFileChild (pPath varchar(200) ) 
BEGIN 
	SELECT ID, Path, Name FROM Services WHERE Path = pPath;
END|


DROP PROCEDURE if exists SelectFileSubFolders;|

CREATE PROCEDURE SelectFileSubFolders (pPath varchar(200)) 
BEGIN 
	DECLARE pPathLike varchar(201);	

	SELECT CONCAT(pPath, '/%') into pPathLike;

	SELECT ID, Path, Name, IsDirectory FROM Services WHERE (Path = pPath or Path like pPathLike) Having IsDirectory = 1;
END|



DROP PROCEDURE if exists UpdateFile;|

CREATE PROCEDURE UpdateFile (pID int unsigned, pIndexTime int unsigned) 
BEGIN 
	UPDATE Services SET IndexTime = pIndexTime WHERE ID = pID; 
END|


DROP PROCEDURE if exists UpdateFileMove;|

CREATE PROCEDURE UpdateFileMove (pID int unsigned, pPath varchar(200), pName varchar(128),  pIndexTime int unsigned) 
BEGIN 
	UPDATE Services SET Path = pPath, Name = pName, IndexTime = pIndexTime WHERE ID = pID;
END|


DROP PROCEDURE if exists UpdateFileMoveChild;|

CREATE PROCEDURE UpdateFileMoveChild (pNewPath varchar(200), pOldPath varchar(200)) 
BEGIN 
	UPDATE Services SET Path = pNewPath WHERE Path = pOldPath; 
END|


DROP PROCEDURE if exists UpdateFileMovePath;|

CREATE PROCEDURE UpdateFileMovePath (pNewPath varchar(200), pOldPath varchar(200)) 
BEGIN 
	UPDATE ServiceMetaData, Services set ServiceMetaData.MetaDataIndexValue = pNewPath WHERE ServiceMetaData.ServiceID = Services.ID AND Services.Path = pOldPath AND ServiceMetaData.MetaDataID = (select ID FROM MetaDataTypes WHERE MetaName = 'File.Path');
END|




DROP PROCEDURE if exists DeleteFile;|

CREATE PROCEDURE DeleteFile (pID int unsigned)
BEGIN 

	DELETE FROM Services WHERE ID = pID;
	DELETE FROM ServiceMetaData WHERE ServiceID = pID;
	DELETE FROM FileContexts WHERE FileID = pID;
	DELETE FROM FilePending WHERE FileID = pID;
	DELETE FROM ServiceLinks WHERE (ServiceID = pID or LinkID = pID);
END|


DROP PROCEDURE if exists DeleteDirectory;|

CREATE PROCEDURE DeleteDirectory (pID int unsigned, pPath varchar(200)) 
BEGIN 
	DECLARE pPathLike varchar(201);	
	
	SELECT CONCAT(pPath, '/%') into pPathLike;
	
	
	DELETE M FROM ServiceMetaData M, Services F WHERE M.ServiceID = F.ID AND ((F.Path = pPath) OR (F.Path like pPathLike));
	DELETE M FROM FileContexts M, Services F WHERE M.FileID = F.ID AND ((F.Path = pPath) OR (F.Path like pPathLike));
	DELETE M FROM FilePending M, Services F WHERE M.FileID = F.ID AND ((F.Path = pPath) OR (F.Path like pPathLike));
	DELETE FROM Services WHERE (Path = pPath) OR (Path like pPathLike);

	DELETE FROM Services WHERE ID = pID;
	DELETE FROM ServiceMetaData WHERE ServiceID = pID;
	DELETE FROM FileContexts WHERE FileID = pID;
	DELETE FROM FilePending WHERE FileID = pID;
	DELETE FROM ServiceLinks WHERE (ServiceID = pID or LinkID = pID);

END|

-- Keywords SPs --


DROP PROCEDURE if exists GetKeywords;|

CREATE PROCEDURE GetKeywords (pID int unsigned) 
BEGIN 
	Select Keyword from ServiceKeywords where ServiceID = pID;
END|


DROP PROCEDURE if exists AddKeyword;|

CREATE PROCEDURE AddKeyword (pID int unsigned, pValue varchar(32)) 
BEGIN 
	insert into ServiceKeywords (ServiceID, Keyword) values (pID, pValue);
END|


DROP PROCEDURE if exists RemoveKeyword;|

CREATE PROCEDURE RemoveKeyword (pID int unsigned, pValue varchar(32)) 
BEGIN 
	delete from ServiceKeywords where ServiceID = pID and Keyword = pValue;
END|


DROP PROCEDURE if exists SearchKeywords;|

CREATE PROCEDURE SearchKeywords (pService varchar(32), pValue varchar(32), pMaxHits int) 
BEGIN 

	Declare pMinServiceTypeID int;
	Declare pMaxServiceTypeID int;

	Set pMinServiceTypeID = GetServiceTypeID (pService);
	Set pMaxServiceTypeID = GetMaxServiceTypeID (pService);

	IF (pService != 'Emails') THEN
		Select S.ID, S.Path, S.Name from Services S, ServiceKeywords K where K.ServiceID = S.ID AND (S.ServiceTypeID between pMinServiceTypeID and pMaxServiceTypeID) and K.Keyword = pValue;
	END IF;
END|



-- Metadata SPs --


DROP PROCEDURE if exists GetMetadata;|

CREATE PROCEDURE GetMetadata (pService varchar(32), pID int unsigned, pMeta varchar(64))
BEGIN 
sproc:BEGIN

	Declare pMID int default -1;
	Declare pDataType int;

	select ID,DataTypeID into pMID, pDataType FROM MetaDataTypes WHERE MetaName = pMeta; 

	IF ((pMid is NULL) or (pMid = -1)) THEN
		leave sproc;
	END IF;

	
	IF ( NOT pService = 'Emails' AND NOT pMeta = 'File.Content') THEN

		CASE pDataType
			WHEN 0 THEN SELECT MetaDataIndexValue FROM ServiceMetaData WHERE ServiceID = pID AND MetaDataID = pMID;

			WHEN 1 THEN SELECT MetaDataValue FROM ServiceMetaData WHERE ServiceID = pID AND MetaDataID = pMID;

			WHEN 2 THEN SELECT MetaDataNumericValue FROM ServiceMetaData WHERE ServiceID = pID AND MetaDataID = pMID;

			WHEN 3 THEN SELECT MetaDataNumericValue FROM ServiceMetaData WHERE ServiceID = pID AND MetaDataID = pMID;
		END CASE;
	END IF;
END;	
END|


DROP PROCEDURE if exists SetMetadata;|

CREATE PROCEDURE SetMetadata (pService varchar(32), pID int unsigned, pMeta varchar(64), pValue Text, pOverwrite bool)
BEGIN 
sproc:BEGIN
	Declare pMID int;
	Declare pDataType int;
	Declare pWriteable bool;

	select ID,DataTypeID, Writeable into pMID, pDataType, pWriteable FROM MetaDataTypes WHERE MetaName = pMeta; 


	If (pWriteable = 1 AND pOverwrite = 0) THEN
		SELECT 'Metadata is not of a writeable type';
		leave sproc;
	END IF;


	IF (pService != 'Emails') THEN

			CASE pDataType
				WHEN 0 THEN INSERT INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataIndexValue) VALUES (pID,pMID,pValue) ON DUPLICATE KEY UPDATE MetaDataIndexValue = pValue;

				WHEN 1 THEN INSERT INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataValue) VALUES (pID,pMID,pValue) ON DUPLICATE KEY UPDATE MetaDataValue = pValue;

				WHEN 2 THEN 
				BEGIN
					Declare pNumeric Double;

					SELECT CAST(pValue as DECIMAL(20,10)) into pNumeric;

					IF ( SELECT pNumeric IS NOT NULL ) THEN
						INSERT INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataNumericValue) VALUES (pID,pMID,pNumeric) ON DUPLICATE KEY UPDATE MetaDataNumericValue = pNumeric;
					ELSE
						SELECT Concat('Invalid numeric format - could not set metadata for field ',pMeta);
					END IF;   	


				END;

				WHEN 3 THEN 
				BEGIN
					Declare pNumeric Double;

					SELECT CAST(pValue as DECIMAL(20,10)) into pNumeric;

					IF ( SELECT pNumeric IS NOT NULL ) THEN
						INSERT INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataNumericValue) VALUES (pID,pMID,pNumeric) ON DUPLICATE KEY UPDATE MetaDataNumericValue = pNumeric;
					ELSE
						SELECT Concat('Invalid numeric format - could not set metadata for field ',pMeta);
					END IF;   	


				END;


			END CASE;
	END IF;
END;	
END|





-- Metadata Type SPs --

DROP PROCEDURE if exists GetMetadataTypeInfo;|

CREATE PROCEDURE  GetMetadataTypeInfo (pType varchar(32))
BEGIN
	SELECT  ID, DataTypeID, Embedded, Writeable  FROM MetaDataTypes where MetaName = pType;
END|




DROP PROCEDURE if exists SelectMetadataTypes;|

CREATE PROCEDURE  SelectMetadataTypes (pClass varchar(16), pWritableOnly bool)
BEGIN

	Declare pClassLike varchar(16);

	SELECT CONCAT(pClass, '.%') into pClassLike;

	IF (SELECT pWritableOnly = 0) THEN    	
		IF (Class = '*') THEN
			SELECT MetaName, ID, DataTypeID, Embedded, Writeable  FROM MetaDataTypes;
		ELSE
			SELECT MetaName, ID, DataTypeID, Embedded, Writeable  FROM MetaDataTypes WHERE MetaName like pClassLike;
		END IF;

	ELSE
		IF (Class = '*') THEN
			SELECT MetaName, ID, DataTypeID, Embedded, Writeable  FROM MetaDataTypes where writeable = 1;
		ELSE
			SELECT MetaName, ID, DataTypeID, Embedded, Writeable  FROM MetaDataTypes WHERE MetaName like pClassLike and  writeable = 1;
		END IF;

	END IF;


END|



DROP PROCEDURE if exists InsertMetadataType;|

CREATE PROCEDURE InsertMetadataType (pMetaName varchar(64), pDataTypeID int, pEmbedded bool, pWriteable bool)
BEGIN
    	INSERT INTO MetaDataTypes (MetaName, DataTypeID, Embedded, Writeable) VALUES (pMetaName, pDataTypeID, pEmbedded, pWriteable); 
END|



-- search SPs


DROP PROCEDURE if exists PrepareQueries;|

CREATE PROCEDURE PrepareQueries ()
BEGIN
	PREPARE SEARCH_TEXT_UNSORTED FROM 'SELECT  DISTINCT F.Path, F.Name FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID AND (F.ServiceTypeID between ? and ?) AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE)  LIMIT ?';
	PREPARE SEARCH_TEXT_SORTED FROM 'SELECT  DISTINCT F.Path, F.Name,   MATCH (M.MetaDataIndexValue) AGAINST (?) FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID  AND (F.ServiceTypeID between ? and ?) AND MATCH (M.MetaDataIndexValue) AGAINST (?) LIMIT ?';
	PREPARE SEARCH_TEXT_SORTED_BOOL FROM 'SELECT  DISTINCT F.Path, F.Name,   MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE) As Relevancy FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID  AND (F.ServiceTypeID between ? and ?) AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE) ORDER BY Relevancy LIMIT ?';
END|



DROP PROCEDURE if exists SearchText;|


CREATE PROCEDURE SearchText (pService varchar(32), pText varchar(255), pMaxHits int, pSorted bool, pBooleanMode bool)
BEGIN
	
	Set @SearchText = pText;
	Set @MaxHits = pMaxHits;
	Set @MinServiceTypeID = GetServiceTypeID (pService);
	Set @MaxServiceTypeID = GetMaxServiceTypeID (pService);
	


	IF (pSorted = 0) THEN
		EXECUTE SEARCH_TEXT_UNSORTED USING @MinServiceTypeID, @MaxServiceTypeID, @SearchText, @MaxHits;
	ELSE 
		IF (pBooleanMode = 0) THEN
			EXECUTE SEARCH_TEXT_SORTED USING @SearchText, @MinServiceTypeID,  @MaxServiceTypeID, @SearchText, @MaxHits;
		ELSE
			EXECUTE SEARCH_TEXT_SORTED_BOOL USING @SearchText, @MinServiceTypeID, @MaxServiceTypeID, @SearchText, @MaxHits;
		END IF;
	END IF; 
END|



DROP PROCEDURE if exists SearchTextMime;|

CREATE PROCEDURE SearchTextMime (pText varchar(255), pMimes varchar(512))
BEGIN
    	SELECT DISTINCT F.Path, F.Name FROM Services F, ServiceMetaData M, ServiceMetaData M2 
		WHERE F.ID = M.ServiceID AND F.ID = M2.ServiceID AND MATCH (M.MetaDataIndexValue) AGAINST (pText IN BOOLEAN MODE) AND 
			M2.MetaDataID = (SELECT ID FROM MetaDataTypes WHERE MetaName = 'File.Format') AND FIND_IN_SET(M2.MetaDataIndexValue, pMimes) AND
				(F.ServiceTypeID between 0 and 4) LIMIT 512;
END|


DROP PROCEDURE if exists SearchTextLocation;|

CREATE PROCEDURE SearchTextLocation (pText varchar(255), pLocation varchar(255))
BEGIN

	Declare pLocationLike varchar(255);

	SELECT CONCAT(pLocation, '/%') into pLocationLike;

	SELECT DISTINCT F.Path, F.Name FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID AND (F.Path like pLocationLike OR F.Path = pLocation) AND MATCH (M.MetaDataIndexValue) AGAINST (pText IN BOOLEAN MODE) AND (F.ServiceTypeID between 0 and 4) LIMIT 512;
END|


DROP PROCEDURE if exists SearchTextMimeLocation;|

CREATE PROCEDURE SearchTextMimeLocation (pText varchar(255), pMimes varchar(255), pLocation varchar(255))
BEGIN

	Declare pLocationLike varchar(255);

	SELECT CONCAT(pLocation, '/%') into pLocationLike;

	SELECT DISTINCT F.Path, F.Name FROM Services F, ServiceMetaData M, ServiceMetaData M2 WHERE F.ID = M.ServiceID AND F.ID = M2.ServiceID AND (F.Path like pLocationLike OR F.Path = pLocation) AND MATCH (M.MetaDataIndexValue) AGAINST (pText IN BOOLEAN MODE) AND (F.ServiceTypeID between 0 and 4) AND M2.MetaDataID = (SELECT ID FROM MetaDataTypes WHERE MetaName = 'File.Format') AND FIND_IN_SET(M2.MetaDataIndexValue, pMimes) LIMIT 512;
END|


-- pending files SPs



DROP PROCEDURE if exists ExistsPendingFiles;|

CREATE PROCEDURE ExistsPendingFiles ()
BEGIN
	select 1 From DUAL Where Exists (select ID from FilePending where Action <> 20);
END|



DROP PROCEDURE if exists InsertPendingFile;|

CREATE PROCEDURE InsertPendingFile (pFileID int ,pAction int,pCounter int,pFileUri varchar(255),pMimeType varchar(80) ,pIsDir bool)
BEGIN
    	INSERT INTO FilePending (FileID, Action, PendingDate, FileUri, MimeType, IsDir) VALUES (pFileID,pAction, UTC_TIMESTAMP() + pCounter, pFileUri,pMimeType,pIsDir);
END|


DROP PROCEDURE if exists GetPendingFiles;|

CREATE PROCEDURE GetPendingFiles ()
BEGIN
	CREATE TEMPORARY TABLE tmpfiles SELECT ID, FileID, FileUri, Action, MimeType, IsDir FROM FilePending WHERE NOT (PendingDate > (UTC_TIMESTAMP() + 0))  AND Action <> 20 ORDER BY ID LIMIT 300;
	
	DELETE FROM FilePending USING tmpfiles, FilePending WHERE FilePending.ID = tmpfiles.ID AND FilePending.Action <> 20;

	SELECT FileID, FileUri, Action, MimeType, IsDir FROM tmpfiles ORDER BY ID;
		
END|


-- removes files that are no longer pending --
DROP PROCEDURE if exists RemovePendingFiles;|

CREATE PROCEDURE RemovePendingFiles ()
BEGIN
	DROP TEMPORARY TABLE if exists tmpfiles;
END|



DROP PROCEDURE if exists CountPendingMetadataFiles;|

CREATE PROCEDURE CountPendingMetadataFiles ()
BEGIN
	select 1 From DUAL Where Exists (select ID from FilePending where Action = 20);
END|





DROP PROCEDURE if exists GetPendingMetadataFiles;|

CREATE PROCEDURE GetPendingMetadataFiles ()
BEGIN
	CREATE TEMPORARY TABLE tmpmetadata SELECT ID, FileID, FileUri, Action, MimeType, IsDir FROM FilePending WHERE NOT (PendingDate > (UTC_TIMESTAMP() + 0))  AND Action = 20 ORDER BY ID LIMIT 100;
	
	DELETE FROM FilePending USING tmpmetadata, FilePending WHERE FilePending.ID = tmpmetadata.ID AND FilePending.Action = 20;

	SELECT FileID, FileUri, Action, MimeType, IsDir FROM tmpmetadata ORDER BY ID;
		
END|


-- removes files that are no longer pending --
DROP PROCEDURE if exists RemovePendingMetadataFiles;|

CREATE PROCEDURE RemovePendingMetadataFiles ()
BEGIN
	DROP TEMPORARY TABLE if exists tmpmetadata;
END|


DROP PROCEDURE if exists SelectPendingByUri;|

CREATE PROCEDURE SelectPendingByUri (pFileUri varchar(255))
BEGIN
	SELECT  FileID, FileUri, Action, MimeType, IsDir FROM FilePending WHERE FileUri = pFileUri;
END|



DROP PROCEDURE if exists UpdatePendingFile;|

CREATE PROCEDURE UpdatePendingFile (pFileUri varchar(255), pAction int, pCounter int)
BEGIN
	UPDATE FilePending SET PendingDate = UTC_TIMESTAMP() + pCounter, Action = pAction WHERE FileUri = pFileUri;
END|



DROP PROCEDURE if exists DeletePendingFile;|

CREATE PROCEDURE DeletePendingFile (pFileUri varchar(255))
BEGIN
	DELETE FROM FilePending WHERE FileUri = pFileUri;
END|





-- Watch SPs --

DROP PROCEDURE if exists GetWatchUri;|

CREATE PROCEDURE GetWatchUri (pID INT)
BEGIN
    	select URI from FileWatches where WatchID = pID;
END|


DROP PROCEDURE if exists GetWatchID;|

CREATE PROCEDURE GetWatchID (pUri varchar(255))
BEGIN
    	select WatchID from FileWatches where URI = pUri;
END|


DROP PROCEDURE if exists GetSubWatches;|

CREATE PROCEDURE GetSubWatches (pUri varchar(255))
BEGIN
	Declare pUriLike varchar(255);

	SELECT CONCAT(pUri, '/%') into pUriLike;

    	select WatchID from FileWatches where URI like pUriLike;
END|


DROP PROCEDURE if exists DeleteWatch;|

CREATE PROCEDURE DeleteWatch (pFileUri varchar(255))
BEGIN
    	delete from FileWatches where URI = pFileUri;
END|


DROP PROCEDURE if exists DeleteSubWatches;|

CREATE PROCEDURE DeleteSubWatches (pFileUri varchar(255))
BEGIN
	Declare pUriLike varchar(255);

	SELECT CONCAT(pFileUri, '/%') into pUriLike;

    	delete from FileWatches where URI like pUriLike;
END|


DROP PROCEDURE if exists InsertWatch;|

CREATE PROCEDURE InsertWatch (pURI varchar(255), pWatchID int)
BEGIN
    	insert into FileWatches (URI, WatchID) values (pURI, pWatchID);
END|
















-- end statement to avoid empty query --
select Null

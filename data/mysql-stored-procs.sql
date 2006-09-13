
-- general purpose functions --


DROP FUNCTION if exists GetServiceName;|

CREATE FUNCTION GetServiceName (pServiceTypeID int)
RETURNS varchar(32)
BEGIN
	Declare result varchar(32) default 'Unknown';

	select TypeName into result From ServiceTypes where TypeID = pServiceTypeID;

	return result;
END|



DROP FUNCTION if exists GetMetaDataName;|

CREATE FUNCTION GetMetaDataName (pMetadataID int)
RETURNS varchar(128)
BEGIN
	Declare result varchar(128) default 'Unknown';

	select MetaName into result From MetaDataTypes where ID = pMetadataID;

	return result;
END|


DROP FUNCTION if exists GetMetaDataTypeID;|

CREATE FUNCTION GetMetaDataTypeID (pMetaData varchar (128))
RETURNS int
BEGIN
	Declare result int default -1;

	select ID into result From MetaDataTypes where MetaName = pMetaData;

	return result;
END|


DROP FUNCTION if exists GetMetaDataType;|

CREATE FUNCTION GetMetaDataType (pMetaData varchar (128))
RETURNS int
BEGIN
	Declare result int default -1;

	select DataTypeID into result From MetaDataTypes where MetaName = pMetaData;

	return result;
END|


DROP FUNCTION if exists GetServiceTypeID;|

CREATE FUNCTION GetServiceTypeID (pService varchar (32))
RETURNS int
BEGIN
	Declare result int default -1;

	select TypeID into result From ServiceTypes where TypeName = pService;

	return result;
END|


DROP FUNCTION if exists GetServiceIDNum;|

CREATE FUNCTION GetServiceIDNum (pPath varchar(200), pName varchar(200))
RETURNS int unsigned
BEGIN
	Declare result int unsigned default 0;

	select ID into result From Services where Path = pPath and Name  = pName;

	return result;
END|


/* where a service has a range of values, this function returns the uppermost ServiceTypeID */
DROP FUNCTION if exists GetMaxServiceTypeID;|

CREATE FUNCTION GetMaxServiceTypeID (pService varchar (32))
RETURNS int
BEGIN
	Declare result int default 0;

	IF (pService = 'Files') THEN
		Set result = GetServiceTypeID ('Other Files');
	ELSEIF (pService = 'VFS Files') THEN
		Set result =  GetServiceTypeID ('VFS Other Files');
	ELSE
		Set result = GetServiceTypeID (pService);		
	END IF;

	return result;
END|



/* prepared queries */

DROP PROCEDURE if exists PrepareQueries;|

CREATE PROCEDURE PrepareQueries ()
BEGIN
	PREPARE SEARCH_TEXT_UNSORTED FROM 'SELECT  DISTINCT F.Path, F.Name FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID AND (F.ServiceTypeID between ? and ?) AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE)  LIMIT ?,?';
	PREPARE SEARCH_TEXT_SORTED FROM 'SELECT  DISTINCT F.Path, F.Name,   MATCH (M.MetaDataIndexValue) AGAINST (?) FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID  AND (F.ServiceTypeID between ? and ?) AND MATCH (M.MetaDataIndexValue) AGAINST (?) LIMIT ?,?';
	PREPARE SEARCH_TEXT_SORTED_BOOL FROM 'SELECT  DISTINCT F.Path, F.Name ,   MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE) As Relevancy FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID  AND (F.ServiceTypeID between ? and ?) AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE) ORDER BY Relevancy LIMIT ?,?';


	PREPARE SEARCH_INDEXED_METADATA FROM 'SELECT  DISTINCT CONCAT(F.Path, \'/\', F.Name) as uri  FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID AND (F.ServiceTypeID between ? and ?) AND M.MetaDataID = ? AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE)  LIMIT ?,?';
	PREPARE SEARCH_STRING_METADATA FROM 'SELECT  DISTINCT CONCAT(F.Path, \'/\', F.Name) as uri  FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID AND (F.ServiceTypeID between ? and ?) AND M.MetaDataID = ? AND M.MetaDataValue = ?  LIMIT ?,?';
	PREPARE SEARCH_NUMERIC_METADATA FROM 'SELECT  DISTINCT CONCAT(F.Path, \'/\', F.Name) as uri  FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID AND (F.ServiceTypeID between ? and ?) AND M.MetaDataID = ? AND M.MetaDataNumericValue = ?  LIMIT ?,?';

	PREPARE SEARCH_FILES_TEXT FROM 'SELECT DISTINCT CONCAT(F.Path, \'/\', F.Name) as uri, GetServiceName(F.ServiceTypeID) as sname, M1.MetaDataIndexValue as mimetype, M2.MetaDataNumericValue as FileSize, M3.MetaDataNumericValue as FileRank, M4.MetaDataNumericValue as  FileModified FROM Services F inner join ServiceMetaData M on F.ID = M.ServiceID LEFT OUTER JOIN ServiceMetaData M1 on F.ID = M1.ServiceID LEFT OUTER JOIN ServiceMetaData M2 on F.ID = M2.ServiceID LEFT OUTER JOIN ServiceMetaData M3 on F.ID = M3.ServiceID LEFT OUTER JOIN ServiceMetaData M4 on F.ID = M4.ServiceID WHERE (F.ServiceTypeID between ? and ?) AND (M1.MetaDataID = GetMetaDataTypeID(''File.Format''))  AND (M2.MetaDataID = GetMetaDataTypeID(''File.Size''))  AND (M3.MetaDataID = GetMetaDataTypeID(''File.Rank''))  AND (M4.MetaDataID = GetMetaDataTypeID(''File.Modified'')) AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE)  LIMIT ?,?';
	PREPARE SEARCH_FILES_TEXT_SORTED FROM 'SELECT  DISTINCT Concat(F.Path, \'/\', F.Name) as uri, GetServiceName(F.ServiceTypeID) as sname, M1.MetaDataIndexValue as mimetype, M2.MetaDataNumericValue as FileSize, M3.MetaDataNumericValue as FileRank, M4.MetaDataNumericValue as  FileModified FROM Services F inner join ServiceMetaData M on F.ID = M.ServiceID LEFT OUTER JOIN ServiceMetaData M1 on F.ID = M1.ServiceID LEFT OUTER JOIN ServiceMetaData M2 on F.ID = M2.ServiceID LEFT OUTER JOIN ServiceMetaData M3 on F.ID = M3.ServiceID LEFT OUTER JOIN ServiceMetaData M4 on F.ID = M4.ServiceID WHERE (F.ServiceTypeID between ? and ?) AND (M1.MetaDataID = GetMetaDataTypeID(''File.Format''))  AND (M2.MetaDataID = GetMetaDataTypeID(''File.Size''))  AND (M3.MetaDataID = GetMetaDataTypeID(''File.Rank''))  AND (M4.MetaDataID = GetMetaDataTypeID(''File.Modified'')) AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE) order by sname, uri  LIMIT ?,?';

	PREPARE SEARCH_MATCHING_FIELDS FROM 'SELECT  DISTINCT GetMetaDataName(M.MetaDataID) as metaname,  M.MetaDataIndexValue FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID AND (M.MetaDataID != GetMetaDataTypeID(''File.Content'')) AND (F.ServiceTypeID between ? and ?) AND F.PATH = ? AND F.NAME = ? AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE)  UNION SELECT  DISTINCT GetMetaDataName(M.MetaDataID) as metaname,  SUBSTRING(M.MetaDataIndexValue, LOCATE(? ,M.MetaDataIndexValue), 30) FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID AND (M.MetaDataID = GetMetaDataTypeID(''File.Content'')) AND (F.ServiceTypeID between ? and ?) AND F.PATH = ? AND F.NAME = ? AND MATCH (M.MetaDataIndexValue) AGAINST (? IN BOOLEAN MODE)';
	
	PREPARE GET_FILES_SERVICE_TYPE FROM 'SELECT  DISTINCT Concat(F.Path, \'/\', F.Name) as uri  FROM Services F WHERE (F.ServiceTypeID between ? and ?) LIMIT ?,?';

	PREPARE GET_FILES_MIME FROM 'SELECT DISTINCT Concat(F.Path, \'/\', F.Name) as uri FROM Services F inner join ServiceMetaData M on F.ID = M.ServiceID WHERE M.MetaDataID = GetMetaDataTypeID(''File.Format'') AND FIND_IN_SET(M.MetaDataIndexValue, ?) AND (F.ServiceTypeID between 0 and 8) LIMIT ?,?';
	PREPARE GET_VFS_FILES_MIME FROM 'SELECT DISTINCT Concat(F.Path, \'/\', F.Name) as uri FROM Services F inner join ServiceMetaData M on F.ID = M.ServiceID WHERE M.MetaDataID = GetMetaDataTypeID(''File.Format'') AND FIND_IN_SET(M.MetaDataIndexValue, ?) AND (F.ServiceTypeID between 9 and 17) LIMIT ?,?';

	PREPARE GET_FILE_MTIME FROM 'SELECT M.MetaDataNumericValue  FROM Services F inner join ServiceMetaData M on F.ID = M.ServiceID WHERE F.Path = ? and F.Name = ? and M.MetaDataID = GetMetaDataTypeID(''File.Modified'') ';

	PREPARE SEARCH_KEYWORDS FROM 'Select Concat(S.Path, ''/'', S.Name) as uri from Services  S INNER JOIN ServiceKeywords K ON K.ServiceID = S.ID WHERE (S.ServiceTypeID between ? and ?) and K.Keyword = ? limit ?,?';

END|


DROP PROCEDURE if exists IndexIDExists;|

CREATE PROCEDURE IndexIDExists (ID int unsigned)
BEGIN
	select 1 from dual where exists (select IndexID from ServiceMetaData where IndexID = ID);
END|

-- service SPs --

DROP PROCEDURE if exists GetServices;|

CREATE PROCEDURE GetServices (pMainServiceOnly Bit) 
BEGIN 


	IF (pMainServiceOnly = 0) THEN
		SELECT TypeName, MetadataClass, Description  FROM ServiceTypes ORDER BY TypeID;
	ELSE
		SELECT TypeName, MetadataClass, Description  FROM ServiceTypes WHERE MainService = 1 ORDER BY TypeID;
	END IF;
END|


DROP PROCEDURE if exists ValidService;|

CREATE PROCEDURE ValidService (pService varchar (32))
BEGIN 
	IF (GetServiceTypeID (pService) > -1) THEN
		select 1;
	ELSE
     		select 0;
	END IF;

END|



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

	Set pServiceTypeID = GetServiceTypeID (pServiceType);

	

	UPDATE sequence SET id=LAST_INSERT_ID(id+1);
	SELECT LAST_INSERT_ID() into pID;

	INSERT INTO Services (ID, Path, Name, ServiceTypeID, IsDirectory, IsLink,  IsServiceSource, Offset, IndexTime) VALUES (pID, pPath,pName,pServiceTypeID,pIsDirectory,pIsLink, pIsServiceSource, pOffset, pIndexTime); 

END|


DROP PROCEDURE if exists DeleteService;|

CREATE PROCEDURE DeleteService (pID int unsigned)
BEGIN 
	
	DELETE FROM Services WHERE ID = pID;
	DELETE FROM ServiceMetaData WHERE ServiceID = pID;
	DELETE FROM ServiceLinks WHERE (ServiceID = pID or LinkID = pID);
	DELETE FROM ServiceKeywords WHERE ServiceID = pID;
	
END|


DROP PROCEDURE if exists DeleteEmbeddedServiceMetadata;|

CREATE PROCEDURE DeleteEmbeddedServiceMetadata (pID int unsigned)
BEGIN 

	DELETE FROM ServiceMetaData WHERE ServiceID = pID AND MetaDataID IN (SELECT ID FROM MetaDataTypes WHERE Embedded = 1);
	
END|


-- File SPs --


DROP PROCEDURE if exists GetFilesByMimeType|

CREATE PROCEDURE GetFilesByMimeType (pMimeTypes varchar(512), pOffset int, pMaxHits int) 
BEGIN 
		
	Set @MimeTypes = pMimeTypes;
	Set @MaxHits = pMaxHits;
	Set @Offset = pOffset;
	
	EXECUTE GET_FILES_MIME USING  @MimeTypes, @Offset, @MaxHits;
END|



DROP PROCEDURE if exists GetVFSFilesByMimeType|

CREATE PROCEDURE GetVFSFilesByMimeType (pMimeTypes varchar(512), pOffset int, pMaxHits int) 
BEGIN 
		
	Set @MimeTypes = pMimeTypes;
	Set @MaxHits = pMaxHits;
	Set @Offset = pOffset;
	
	EXECUTE GET_VFS_FILES_MIME USING  @MimeTypes, @Offset, @MaxHits;
END|


DROP PROCEDURE if exists GetFileMTime|

CREATE PROCEDURE GetFileMTime (pPath varchar(200), pName varchar(200)) 
BEGIN 
	SELECT M.MetaDataNumericValue  FROM Services F inner join ServiceMetaData M on F.ID = M.ServiceID WHERE F.Path = pPath and F.Name = pName and M.MetaDataID = GetMetaDataTypeID('File.Modified');
END|




DROP PROCEDURE if exists GetFileContents|

CREATE PROCEDURE GetFileContents (pPath varchar(200), pName varchar(200), pOffset int, pLength int) 
BEGIN 
	
	SELECT SUBSTRING(MetaDataIndexValue, pOffset, pLength) FROM ServiceMetaData WHERE ServiceID = GetServiceIDNum(pPath, pName) AND MetaDataID = GetMetaDataTypeID('File.Content');
END|

DROP PROCEDURE if exists SearchFileContents|

CREATE PROCEDURE SearchFileContents (pPath varchar(200), pName varchar(200), ptext varchar(128), pLength int) 
BEGIN 
	
	SELECT SUBSTRING(MetaDataIndexValue, LOCATE(ptext ,MetaDataIndexValue), pLength)  FROM ServiceMetaData WHERE ServiceID = GetServiceIDNum(pPath, pName) AND MetaDataID = GetMetaDataTypeID('File.Content');
END|


DROP PROCEDURE if exists GetFilesByServiceType|

CREATE PROCEDURE GetFilesByServiceType (pService varchar(64), pOffset int, pMaxHits int) 
BEGIN 
		
	Set @MaxHits = pMaxHits;
	Set @Offset = pOffset;
	Set @MinServiceTypeID = GetServiceTypeID (pService);
	Set @MaxServiceTypeID = GetMaxServiceTypeID (pService);
	
	EXECUTE GET_FILES_SERVICE_TYPE USING  @MinServiceTypeID, @MaxServiceTypeID, @Offset, @MaxHits;
END|



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
	DELETE FROM ServiceKeywords WHERE (ServiceID = pID);
END|


DROP PROCEDURE if exists DeleteDirectory;|

CREATE PROCEDURE DeleteDirectory (pID int unsigned, pPath varchar(200)) 
BEGIN 
	DECLARE pPathLike varchar(201);	
	
	SELECT CONCAT(pPath, '/%') into pPathLike;
	
	
	DELETE M FROM ServiceMetaData M, Services F WHERE M.ServiceID = F.ID AND ((F.Path = pPath) OR (F.Path like pPathLike));
	DELETE M FROM FileContexts M, Services F WHERE M.FileID = F.ID AND ((F.Path = pPath) OR (F.Path like pPathLike));
	DELETE M FROM FilePending M, Services F WHERE M.FileID = F.ID AND ((F.Path = pPath) OR (F.Path like pPathLike));
	DELETE M FROM ServiceKeywords M, Services F WHERE M.ServiceID = F.ID AND ((F.Path = pPath) OR (F.Path like pPathLike));
	DELETE FROM Services WHERE (Path = pPath) OR (Path like pPathLike);

	DELETE FROM Services WHERE ID = pID;
	DELETE FROM ServiceMetaData WHERE ServiceID = pID;
	DELETE FROM FileContexts WHERE FileID = pID;
	DELETE FROM FilePending WHERE FileID = pID;
	DELETE FROM ServiceLinks WHERE (ServiceID = pID or LinkID = pID);
	DELETE FROM ServiceKeywords WHERE (ServiceID = pID);

END|

-- Keywords SPs --


DROP PROCEDURE if exists GetKeywordList;|

CREATE PROCEDURE GetKeywordList (pService varchar(32)) 
BEGIN 
	Declare pMinServiceTypeID int;
	Declare pMaxServiceTypeID int;

	Set pMinServiceTypeID = GetServiceTypeID (pService);
	Set pMaxServiceTypeID = GetMaxServiceTypeID (pService);

	IF (pService != 'Emails') THEN
		Select distinct K.Keyword, count(*) from Services S, ServiceKeywords K where K.ServiceID = S.ID AND (S.ServiceTypeID between pMinServiceTypeID and pMaxServiceTypeID) group by K.Keyword order by 2,1 desc;
	END IF;
END|



DROP PROCEDURE if exists GetKeywords;|

CREATE PROCEDURE GetKeywords (pPath varchar(200), pName varchar(200))
BEGIN 
	Select Keyword from ServiceKeywords where ServiceID = GetServiceIDNum(pPath, pName);
END|


DROP PROCEDURE if exists AddKeyword;|

CREATE PROCEDURE AddKeyword (pPath varchar(200), pName varchar(200), pValue varchar(32)) 
BEGIN 
	insert into ServiceKeywords (ServiceID, Keyword) values (GetServiceIDNum(pPath, pName), pValue);
END|


DROP PROCEDURE if exists RemoveKeyword;|

CREATE PROCEDURE RemoveKeyword (pPath varchar(200), pName varchar(200), pValue varchar(32)) 
BEGIN 
	delete from ServiceKeywords where ServiceID = GetServiceIDNum(pPath, pName) and Keyword = pValue;
END|


DROP PROCEDURE if exists RemoveAllKeywords;|

CREATE PROCEDURE RemoveAllKeywords (pPath varchar(200), pName varchar(200)) 
BEGIN 
	delete from ServiceKeywords where ServiceID = GetServiceIDNum(pPath, pName);
END|


DROP PROCEDURE if exists SearchKeywords;|

CREATE PROCEDURE SearchKeywords (pService varchar(32), pValue varchar(255), pOffset int, pMaxHits int) 
BEGIN 

	Set @MinServiceTypeID = GetServiceTypeID (pService);
	Set @MaxServiceTypeID = GetMaxServiceTypeID (pService);
	Set @keyword = pValue;
	Set @MaxHits = pMaxHits;
	Set @Offset = pOffset;

	EXECUTE SEARCH_KEYWORDS USING @MinServiceTypeID, @MaxServiceTypeID, @keyword, @Offset, @MaxHits;
	
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


DROP PROCEDURE if exists SetKeywordMetadata;|

CREATE PROCEDURE SetKeywordMetadata (pService varchar(32), pID int unsigned, pValue Text)
BEGIN 
	Declare pMID int;

	select ID into pMID FROM MetaDataTypes WHERE MetaName = 'Keywords'; 

	IF (pService != 'Emails') THEN
		INSERT INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataIndexValue) VALUES (pID,pMID,pValue) ON DUPLICATE KEY UPDATE MetaDataIndexValue = pValue;
	END IF;
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



DROP PROCEDURE if exists SelectMetadataClasses;|

CREATE PROCEDURE  SelectMetadataClasses ()
BEGIN
	SELECT DISTINCT SUBSTRING_INDEX(MetaName, '.', 1) FROM MetaDataTypes;
END|


DROP PROCEDURE if exists InsertMetadataType;|

CREATE PROCEDURE InsertMetadataType (pMetaName varchar(64), pDataTypeID int, pEmbedded bool, pWriteable bool)
BEGIN
    	INSERT INTO MetaDataTypes (MetaName, DataTypeID, Embedded, Writeable) VALUES (pMetaName, pDataTypeID, pEmbedded, pWriteable); 
END|



-- search SPs





DROP PROCEDURE if exists SearchFilesText;|

CREATE PROCEDURE SearchFilesText (pText varchar(255), pOffset int, pMaxHits int, pSorted bool)
BEGIN
	
	Set @SearchText = pText;
	Set @MaxHits = pMaxHits;
	Set @MinServiceTypeID = GetServiceTypeID ('Files');
	Set @MaxServiceTypeID = GetMaxServiceTypeID ('Files');
	Set @Offset = pOffset;

	IF (pSorted = 0) THEN
		EXECUTE SEARCH_FILES_TEXT USING @MinServiceTypeID, @MaxServiceTypeID, @SearchText, @Offset, @MaxHits;
	ELSE 
		EXECUTE SEARCH_FILES_TEXT_SORTED USING @MinServiceTypeID, @MaxServiceTypeID, @SearchText, @Offset, @MaxHits;
	END IF; 
END|




DROP PROCEDURE if exists SearchMetaData;|

CREATE PROCEDURE SearchMetaData (pService varchar(32), pMetaData varchar(128), pText varchar(255), pOffset int, pMaxHits int)
BEGIN
	
	Set @SearchText = pText;
	Set @MaxHits = pMaxHits;
	Set @SearchMetadataID = GetMetaDataTypeID (pMetaData);
	Set @MinServiceTypeID = GetServiceTypeID (pService);
	Set @MaxServiceTypeID = GetMaxServiceTypeID (pService);
	Set @Offset = pOffset;
	
	IF (GetMetaDataType(pMetaData) = 0) THEN
		EXECUTE SEARCH_INDEXED_METADATA  USING @MinServiceTypeID, @MaxServiceTypeID, @SearchMetadataID,  @SearchText, @Offset, @MaxHits;
	END IF;
		
END|


DROP PROCEDURE if exists SearchMatchingMetaData;|

CREATE PROCEDURE SearchMatchingMetaData (pService varchar(32), pPath varchar(255), pName varchar(255), pText varchar(255))
BEGIN
	
	Set @SearchText = pText;
	Set @Path = pPath;
	Set @Name = pName;
	Set @MinServiceTypeID = GetServiceTypeID (pService);
	Set @MaxServiceTypeID = GetMaxServiceTypeID (pService);

	EXECUTE SEARCH_MATCHING_FIELDS  USING @MinServiceTypeID, @MaxServiceTypeID,@Path, @Name,  @SearchText, @SearchText, @MinServiceTypeID, @MaxServiceTypeID,@Path, @Name,  @SearchText;

		
END|




DROP PROCEDURE if exists SearchText;|


CREATE PROCEDURE SearchText (pService varchar(32), pText varchar(255), pOffset int, pMaxHits int, pSorted bool, pBooleanMode bool)
BEGIN
	
	Set @SearchText = pText;
	Set @MaxHits = pMaxHits;
	Set @MinServiceTypeID = GetServiceTypeID (pService);
	Set @MaxServiceTypeID = GetMaxServiceTypeID (pService);
	Set @Offset = pOffset;


	IF (pSorted = 0) THEN
		EXECUTE SEARCH_TEXT_UNSORTED USING @MinServiceTypeID, @MaxServiceTypeID, @SearchText, @Offset, @MaxHits;
	ELSE 
		IF (pBooleanMode = 0) THEN
			EXECUTE SEARCH_TEXT_SORTED USING @SearchText, @MinServiceTypeID,  @MaxServiceTypeID, @SearchText, @Offset, @MaxHits;
		ELSE
			EXECUTE SEARCH_TEXT_SORTED_BOOL USING @SearchText, @MinServiceTypeID, @MaxServiceTypeID, @SearchText, @Offset, @MaxHits;
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

	SELECT DISTINCT F.Path, F.Name FROM Services F, ServiceMetaData M WHERE F.ID = M.ServiceID AND (F.Path like pLocationLike OR F.Path = pLocation) AND MATCH (M.MetaDataIndexValue) AGAINST (pText IN BOOLEAN MODE) AND (F.ServiceTypeID between 0 and 8) LIMIT 512;
END|


DROP PROCEDURE if exists SearchTextMimeLocation;|

CREATE PROCEDURE SearchTextMimeLocation (pText varchar(255), pMimes varchar(255), pLocation varchar(255))
BEGIN

	Declare pLocationLike varchar(255);

	SELECT CONCAT(pLocation, '/%') into pLocationLike;

	SELECT DISTINCT F.Path, F.Name FROM Services F, ServiceMetaData M, ServiceMetaData M2 WHERE F.ID = M.ServiceID AND F.ID = M2.ServiceID AND (F.Path like pLocationLike OR F.Path = pLocation) AND MATCH (M.MetaDataIndexValue) AGAINST (pText IN BOOLEAN MODE) AND (F.ServiceTypeID between 0 and 8) AND M2.MetaDataID = (SELECT ID FROM MetaDataTypes WHERE MetaName = 'File.Format') AND FIND_IN_SET(M2.MetaDataIndexValue, pMimes) LIMIT 512;
END|


-- Playlists SPs


DROP PROCEDURE if exists RenamePlayList;|

CREATE PROCEDURE RenamePlayList (pOldName varchar(255), pNewName varchar(255))
BEGIN

	Update Services set Name = pNewName where Name = pOldName and ServiceTypeID = GetServiceTypeID('Playlists');
	Update ServiceMetaData set MetaDataIndexValue = pNewName where MetaDataID = GetMetaDataTypeID('Playlist.Name') and  MetaDataIndexValue = pOldName;

END|


DROP PROCEDURE if exists GetAllPlaylists;|

CREATE PROCEDURE GetAllPlaylists ()
BEGIN
	select distinct Name from Services where ServiceTypeID = GetServiceTypeID('Playlists');
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



-- stats
DROP PROCEDURE if exists GetStats;|

CREATE PROCEDURE GetStats ()
BEGIN
	select 'Total files indexed', 
		count(*) as n, NULL
        from Services where ServiceTypeID between 0 and 8 
	union
	select  T.TypeName, 
		count(*) as n,
		ROUND(COUNT(*)
                / (SELECT COUNT(*) FROM Services)
                * 100, 2)
		from Services S, ServiceTypes T where S.ServiceTypeID = T.TypeID group by T.TypeName; 
END|














-- end statement to avoid empty query --
select Null

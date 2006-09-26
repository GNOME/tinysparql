ValidService select 1 where exists ( select TypeID From ServiceTypes where TypeName = ?);

GetServiceType select TypeName From ServiceTypes where TypeID = ?;

GetMetaDataName	  SELECT MetaName From MetaDataTypes where ID = ?;

GetMetaDataTypeID select ID From MetaDataTypes where MetaName = ?;

GetMetaDataType select DataTypeID From MetaDataTypes where MetaName = ?;

GetServiceTypeID select TypeID From ServiceTypes where TypeName = ?;

GetServiceIDNum select ID From Services where Path = ? and Name  = ?;

GetServiceTypeIDForFile select ServiceTypeID FROM Services where ID = ?;

GetFilesByServiceType SELECT  DISTINCT F.Path || '/' || F.Name as uri  FROM Services F WHERE (F.ServiceTypeID between ? and ?) LIMIT ?,?;

GetFileByID SELECT  DISTINCT Path , Name as uri  FROM Services WHERE ID = ?;

GetFileMTime SELECT M.MetaDataNumericValue  FROM Services F inner join ServiceMetaData M on F.ID = M.ServiceID WHERE F.Path = ? and F.Name = ? and M.MetaDataID = (select ID From MetaDataTypes where MetaName ='File.Modified');

GetMainServices SELECT TypeName, MetadataClass, Description  FROM ServiceTypes WHERE MainService = 1 ORDER BY TypeID;

GetServices SELECT TypeName, MetadataClass, Description  FROM ServiceTypes ORDER BY TypeID;

GetServiceID SELECT ID, IndexTime, IsDirectory FROM Services WHERE Path = ? AND Name = ?;

GetNewID SELECT OptionValue FROM Options WHERE OptionKey = 'Sequence';

UpdateNewID UPDATE Options set OptionValue = ? WHERE OptionKey = 'Sequence';

CreateService INSERT INTO Services (ID, Path, Name, ServiceTypeID, IsDirectory, IsLink,  IsServiceSource, Offset, IndexTime) VALUES (?,?,?,?,?,?,?,?,?); 

DeleteService1 	DELETE FROM Services WHERE ID = ?;
DeleteService2 	DELETE FROM ServiceMetaData WHERE ServiceID = ?;
DeleteService3 	DELETE FROM ServiceLinks WHERE (ServiceID = ? or LinkID = ?);
DeleteService4 	DELETE FROM ServiceKeywords WHERE ServiceID = ?;

DeleteEmbeddedServiceMetadata DELETE FROM ServiceMetaData WHERE ServiceID = ? AND MetaDataID IN (SELECT ID FROM MetaDataTypes WHERE Embedded = 1);
	
SelectFileChild SELECT ID, Path, Name FROM Services WHERE Path = ?;

SelectFileSubFolders SELECT ID, Path, Name, IsDirectory FROM Services WHERE (Path = ?  or Path glob ?) And IsDirectory = 1;

SelectSubFileIDs SELECT ID FROM Services WHERE (Path = ?  or Path glob ?);

UpdateFile UPDATE Services SET IndexTime = ? WHERE ID = ?; 

UpdateFileMove 	UPDATE Services SET Path = ?, Name = ?, IndexTime = ? WHERE ID = ?;

UpdateFileMoveChild UPDATE Services SET Path = ? WHERE Path = ?; 

UpdateFileMovePath UPDATE ServiceMetaData set MetaDataIndexValue = ? WHERE ServiceID in (select ID FROM Services where Path = ?) AND MetaDataID = (select ID FROM MetaDataTypes WHERE MetaName = 'File.Path');

DeleteFile1 DELETE FROM Services WHERE ID = ?;
DeleteFile2 DELETE FROM ServiceMetaData WHERE ServiceID = ?;
DeleteFile3 DELETE FROM FilePending WHERE FileID = ?;
DeleteFile4 DELETE FROM ServiceLinks WHERE (ServiceID = ? or LinkID = ?);
DeleteFile5 DELETE FROM ServiceKeywords WHERE (ServiceID = ?);

DeleteDirectory1 DELETE FROM ServiceMetaData  WHERE ServiceID  in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));
DeleteDirectory2 DELETE FROM FilePending  WHERE FileID in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));
DeleteDirectory3 DELETE FROM ServiceKeywords  WHERE ServiceID in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));
DeleteDirectory4 DELETE FROM Services WHERE (Path = ?) OR (Path glob ?);
DeleteDirectory5 DELETE FROM Services WHERE ID = ?;
DeleteDirectory6 DELETE FROM ServiceMetaData WHERE ServiceID = ?;
DeleteDirectory7 DELETE FROM FilePending WHERE FileID = ?;
DeleteDirectory8 DELETE FROM ServiceLinks WHERE (ServiceID = ? or LinkID = ?);
DeleteDirectory9 DELETE FROM ServiceKeywords WHERE (ServiceID = ?);

SaveFileContents REPLACE into ServiceContents (ServiceID, Content, ContainsWordScores) values (?,?,?);

GetFileContents Select uncompress (Content), ContainsWordScores from ServiceContents where ServiceID = ? and Content is not null

GetKeywordList Select distinct K.Keyword, count(*) from Services S, ServiceKeywords K where K.ServiceID = S.ID AND (S.ServiceTypeID between ? and ?) group by K.Keyword order by 2,1 desc;

GetKeywords Select Keyword from ServiceKeywords where ServiceID = (select ID From Services where Path = ? and Name  = ?);

AddKeyword insert into ServiceKeywords (ServiceID, Keyword) values ((select ID From Services where Path = ? and Name  = ?), ?);

RemoveKeyword delete from ServiceKeywords where ServiceID = (select ID From Services where Path = ? and Name  = ?) and Keyword = ?;

RemoveAllKeywords delete from ServiceKeywords where ServiceID = (select ID From Services where Path = ? and Name  = ?);

SearchKeywords Select Distinct S.Path || '/' || S.Name as uri  from Services  S INNER JOIN ServiceKeywords K ON K.ServiceID = S.ID WHERE (S.ServiceTypeID between ? and ?) and K.Keyword = ? limit ?,?;

GetAllIndexable  SELECT S.MetaDataIndexValue, M.Weight FROM ServiceMetaData S, MetaDataTypes M WHERE  S.MetaDataID = M.ID AND S.ServiceID = ? And S.MetaDataIndexValue is not null and M.DatatypeID = 0;

GetMetadataIndex SELECT MetaDataIndexValue FROM ServiceMetaData WHERE ServiceID = ? AND MetaDataID = (select ID from MetaDataTypes where MetaName = ?);

GetMetadataString SELECT MetaDataValue FROM ServiceMetaData WHERE ServiceID = ? AND MetaDataID = (select ID from MetaDataTypes where MetaName = ?);

GetMetadataNumeric SELECT MetaDataNumericValue FROM ServiceMetaData WHERE ServiceID = ? AND MetaDataID = (select ID from MetaDataTypes where MetaName = ?);

SetMetadataIndex REPLACE INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataIndexValue) VALUES (?,?,?);

SetMetadataString REPLACE INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataValue) VALUES (?,?,?);

SetMetadataNumeric REPLACE INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataNumericValue) VALUES (?,?,?);

GetMetadataTypeInfo SELECT  ID, DataTypeID, Embedded, Writeable, Weight  FROM MetaDataTypes where MetaName = ?;

GetMetadataTypes SELECT MetaName, ID, DataTypeID, Embedded, Writeable, Weight  FROM MetaDataTypes;

GetMetadataTypesLike SELECT MetaName, ID, DataTypeID, Embedded, Writeable, Weight  FROM MetaDataTypes WHERE MetaName glob ?;

GetWriteableMetadataTypes SELECT MetaName, ID, DataTypeID, Embedded, Writeable, Weight  FROM MetaDataTypes where writeable = 1;

GetWriteableMetadataTypesLike SELECT MetaName, ID, DataTypeID, Embedded, Writeable, Weight  FROM MetaDataTypes WHERE MetaName glob ? and  writeable = 1;

SelectMetadataClasses SELECT DISTINCT MetaName FROM MetaDataTypes;

InsertMetadataType INSERT INTO MetaDataTypes (MetaName, DataTypeID, Embedded, Writeable) VALUES (?,?,?,?); 

ExistsPendingFiles select count (*) from FilePending where Action <> 20;

InsertPendingFile INSERT INTO FilePending (FileID, Action, PendingDate, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID) VALUES (?,?,?,?,?,?,?,?,?,?);

CountPendingMetadataFiles select count (*) from FilePending where Action = 20;

SelectPendingByUri SELECT  FileID, FileUri, Action, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM FilePending WHERE FileUri = ?;

UpdatePendingFile UPDATE FilePending SET PendingDate = ?, Action = ? WHERE FileUri = ?;

DeletePendingFile DELETE FROM FilePending WHERE FileUri = ?;

GetWatchUri select URI from FileWatches where WatchID = ?;

GetWatchID select WatchID from FileWatches where URI = ?;

GetSubWatches select WatchID from FileWatches where URI glob ?;

DeleteWatch delete from FileWatches where URI = ?;

DeleteSubWatches delete from FileWatches where URI glob ?;

InsertWatch insert into FileWatches (URI, WatchID) values (?,?);

GetStats select 'Total files indexed', 	count(*) as n from Services where ServiceTypeID between 0 and 8 union select T.TypeName, count(*) as n from Services S, ServiceTypes T where S.ServiceTypeID = T.TypeID group by T.TypeName; 

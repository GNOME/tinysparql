ValidService select 1 where exists ( select TypeID From ServiceTypes where TypeName = ?);

GetServiceType select TypeName From ServiceTypes where TypeID = ?;

GetMetaDataName	  SELECT MetaName From MetaDataTypes where ID = ?;

GetMetaDataTypeID select ID From MetaDataTypes where MetaName = ?;

GetMetaDataType select DataTypeID From MetaDataTypes where MetaName = ?;

GetServiceTypeID select TypeID From ServiceTypes where TypeName = ?;

GetServiceIDNum select ID From Services where Path = ? and Name  = ?;

GetServiceTypeIDForFile select ServiceTypeID FROM Services where ID = ?;

GetFilesByServiceType SELECT  DISTINCT F.Path || '/' || F.Name as uri  FROM Services F WHERE (F.ServiceTypeID between ? and ?) LIMIT ?,?;

GetFileByID  SELECT  DISTINCT Path , Name, Mime   FROM Services WHERE ID = ?;

GetFileByID2  SELECT DISTINCT (Path || '/' || Name) as uri, GetServiceName (ServiceTypeID), Mime FROM Services WHERE ID = ?;

GetFileMTime SELECT M.MetaDataValue  FROM Services F inner join ServiceNumericMetaData M on F.ID = M.ServiceID WHERE F.Path = ? and F.Name = ? and M.MetaDataID = (select ID From MetaDataTypes where MetaName ='File:Modified');

GetServices SELECT TypeName, MetadataClass, Description  FROM ServiceTypes WHERE MainService = ? ORDER BY TypeID;

GetServiceID SELECT ID, IndexTime, IsDirectory, ServiceTypeID FROM Services WHERE Path = ? AND Name = ?;

GetNewID SELECT OptionValue FROM Options WHERE OptionKey = 'Sequence';
UpdateNewID UPDATE Options set OptionValue = ? WHERE OptionKey = 'Sequence';

GetUpdateCount SELECT OptionValue FROM Options WHERE OptionKey = 'UpdateCount';
SetUpdateCount UPDATE Options set OptionValue = ?  WHERE OptionKey = 'UpdateCount';

CreateService INSERT INTO Services (ID, Path, Name, ServiceTypeID, Mime, Size, IsDirectory, IsLink, Offset, IndexTime) VALUES (?,?,?,?,?,?,?,?,?,?); 

DeleteService1 	DELETE FROM Services WHERE ID = ?;
DeleteService2 	DELETE FROM ServiceMetaData WHERE ServiceID = ?;
DeleteService3 	DELETE FROM ServiceLinks WHERE (ServiceID = ? or LinkID = ?);
DeleteService4 	DELETE FROM ServiceKeywordMetaData WHERE ServiceID = ?;
DeleteService5 	DELETE FROM ServiceMetaDataDisplay WHERE ServiceID = ?;
DeleteService6 	DELETE FROM ServiceNumericMetaData WHERE ServiceID = ?;
DeleteService7 	DELETE FROM ServiceBlobMetaData WHERE ServiceID = ?;


MarkEmbeddedServiceMetadata1 update ServiceMetaData set DeleteFlag = 1 where ServiceID = ? AND EmbeddedFlag = 1;
MarkEmbeddedServiceMetadata2 update ServiceNumericMetaData set DeleteFlag = 1 where ServiceID = ? AND EmbeddedFlag = 1;
MarkEmbeddedServiceMetadata3 update ServiceKeywordMetaData set DeleteFlag = 1 where ServiceID = ? AND EmbeddedFlag = 1;
MarkEmbeddedServiceMetadata4 update ServiceBlobMetaData set DeleteFlag = 1 where ServiceID = ? AND EmbeddedFlag = 1;

DeleteEmbeddedServiceMetadata1 DELETE FROM ServiceMetaData WHERE ServiceID = ? AND DeleteFlag = 1 AND EmbeddedFlag = 1;
DeleteEmbeddedServiceMetadata2 DELETE FROM ServiceNumericMetaData WHERE ServiceID = ? AND DeleteFlag = 1 AND EmbeddedFlag = 1;
DeleteEmbeddedServiceMetadata3 DELETE FROM ServiceBlobMetaData WHERE ServiceID = ? AND DeleteFlag = 1 AND EmbeddedFlag = 1;
DeleteEmbeddedServiceMetadata4 DELETE FROM ServiceKeywordMetaData WHERE ServiceID = ? AND DeleteFlag = 1 AND EmbeddedFlag = 1;

SelectFileChild SELECT ID, Path, Name FROM Services WHERE Path = ?;

SelectFileSubFolders SELECT ID, Path, Name, IsDirectory FROM Services WHERE (Path = ?  or Path glob ?) And IsDirectory = 1;

SelectSubFileIDs SELECT ID FROM Services WHERE (Path = ?  or Path glob ?);

UpdateFile UPDATE Services SET IndexTime = ? WHERE ID = ?; 

UpdateFileMove 	UPDATE Services SET Path = ?, Name = ?, IndexTime = ? WHERE ID = ?;

UpdateFileMoveChild UPDATE Services SET Path = ? WHERE Path = ?; 

UpdateFileMovePath UPDATE ServiceMetaData set MetaDataValue = ? WHERE ServiceID in (select ID FROM Services where Path = ?) AND MetaDataID = (select ID FROM MetaDataTypes WHERE MetaName = 'File:Path');

DeleteFile1 DELETE FROM Services WHERE ID = ?;
DeleteFile2 DELETE FROM ServiceMetaData WHERE ServiceID = ?;
DeleteFile3 DELETE FROM FilePending WHERE FileID = ?;
DeleteFile4 DELETE FROM ServiceLinks WHERE (ServiceID = ? or LinkID = ?);
DeleteFile5 DELETE FROM ServiceKeywordMetaData WHERE (ServiceID = ?);
DeleteFile6 DELETE FROM ServiceMetaDataDisplay WHERE ServiceID = ?;
DeleteFile7 DELETE FROM ServiceNumericMetaData WHERE ServiceID = ?;
DeleteFile8 DELETE FROM ServiceBlobMetaData WHERE ServiceID = ?;
DeleteFile9 DELETE FROM ServiceMetaDataDisplay WHERE ServiceID = ?;
DeleteFile10 DELETE FROM ServiceWords WHERE ServiceID = ?;

DeleteDirectory1 DELETE FROM ServiceMetaData  WHERE ServiceID  in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));
DeleteDirectory2 DELETE FROM FilePending  WHERE FileID in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));
DeleteDirectory3 DELETE FROM ServiceKeywordMetaData  WHERE ServiceID in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));
DeleteDirectory4 DELETE FROM Services WHERE (Path = ?) OR (Path glob ?);
DeleteDirectory5 DELETE FROM ServiceMetaDataDisplay  WHERE ServiceID  in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));
DeleteDirectory6 DELETE FROM ServiceNumericMetaData  WHERE ServiceID  in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));
DeleteDirectory7 DELETE FROM ServiceBlobMetaData  WHERE ServiceID  in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));
DeleteDirectory8 DELETE FROM ServiceMetaDataDisplay  WHERE ServiceID  in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));
DeleteDirectory9 DELETE FROM ServiceWords  WHERE ServiceID  in (select ID FROM Services F where (F.Path = ?) OR (F.Path glob ?));

SaveFileContents REPLACE into ServiceContents (ServiceID, Content, ContainsWordScores) values (?,?,?);
DeleteFileContents DELETE FROM ServiceContents where ServiceID = ?;
GetFileContents Select uncompress (Content), ContainsWordScores from ServiceContents where ServiceID = ? and Content is not null

GetKeywordList Select distinct K.MetaDataValue, count(*) as totalcount from Services S, ServiceKeywordMetaData K where K.ServiceID = S.ID AND (S.ServiceTypeID between ? and ?) and K.MetaDataId in (select ID from MetadataTypes where MetaName in ('DC:Keywords', 'Image:Keywords', 'Doc:Keywords')) group by K.MetaDataValue order by totalcount desc, K.MetaDataValue asc;
GetKeywords Select MetaDataValue from ServiceKeywordMetaData where ServiceID = (select ID From Services where Path = ? and Name  = ?) and MetaDataId in (select ID from MetadataTypes where MetaName in ('DC:Keywords', 'Image:Keywords', 'Doc:Keywords'));
AddKeyword insert into ServiceKeywordMetaData (ServiceID, MetaDataID, MetaDataValue) values ((select ID From Services where Path = ? and Name  = ?), (select ID from MetadataTypes where MetaName = 'DC:Keywords') ,?);
AddEmbeddedKeyword insert into ServiceKeywordMetaData (ServiceID, MetaDataID, MetaDataValue, EmbeddedFlag) values (?,(select ID from MetadataTypes where MetaName = ?),?,1) ;
RemoveKeyword delete from ServiceKeywordMetaData where ServiceID = (select ID From Services where Path = ? and Name  = ?) and MetaDataID = (select ID from MetadataTypes where MetaName = 'DC:Keywords') and MetaDataValue = ?;
RemoveAllKeywords delete from ServiceKeywordMetaData where ServiceID = (select ID From Services where Path = ? and Name  = ?)  and MetaDataID = (select ID from MetadataTypes where MetaName = 'DC:Keywords');
SearchKeywords Select Distinct S.Path || '/' || S.Name as uri  from Services  S INNER JOIN ServiceKeywordMetaData K ON K.ServiceID = S.ID WHERE (S.ServiceTypeID between ? and ?)  and MetaDataID = (select ID from MetadataTypes where MetaName = 'DC:Keywords') and K.MetaDataValue = ? limit ?,?;

GetAllIndexable SELECT S.MetaDataValue as MetaValue, M.Weight as MetaWeight FROM ServiceMetaData S, MetaDataTypes M WHERE  S.MetaDataID = M.ID AND S.ServiceID = ? And S.MetaDataValue is not null and M.DatatypeID = 0;
GetAllIndexableKeywords SELECT K.MetaDataValue as MetaValue, M.Weight as MetaWeight FROM ServiceKeywordMetaData K, MetaDataTypes M WHERE  K.MetaDataID = M.ID AND K.ServiceID = ? And K.MetaDataValue is not null and M.DatatypeID = 5;

GetMetadataBlob SELECT MetaDataValue, BlobLength FROM ServiceBlobMetaData WHERE ServiceID = ? AND MetaDataID = (select ID from MetaDataTypes where MetaName = ?);
GetMetadataKeyword SELECT MetaDataValue FROM ServiceKeywordMetaData WHERE ServiceID = ? AND MetaDataID = (select ID from MetaDataTypes where MetaName = ?);
GetMetadataString SELECT MetaDataValue FROM ServiceMetaData WHERE ServiceID = ? AND MetaDataID = (select ID from MetaDataTypes where MetaName = ?);
GetMetadataNumeric SELECT MetaDataValue FROM ServiceNumericMetaData WHERE ServiceID = ? AND MetaDataID = (select ID from MetaDataTypes where MetaName = ?);
GetMetadataDisplay SELECT MetaDataValue FROM ServiceMetaDataDisplay WHERE ServiceID = ? AND MetaDataID = (select ID from MetaDataTypes where MetaName = ?);

DeleteAllDisplayMetadata DELETE FROM ServiceMetaDataDisplay where ServiceID = ?;
GetAllDisplayMetadataTypes Select distinct S.MetaDataId, M.MetaName, M.DataTypeID  from ServiceMetaData S, MetaDataTypes M where S.ServiceID = ? and S.MetaDataID = M.ID and M.MultipleValues=1 union Select distinct S.MetaDataId, M.MetaName, M.DataTypeID  from ServiceKeywordMetaData S, MetaDataTypes M where S.ServiceID = ? and S.MetaDataID = M.ID and M.MultipleValues=1 union Select distinct S.MetaDataId, M.MetaName, M.DataTypeID  from ServiceNumericMetaData S, MetaDataTypes M where S.ServiceID = ? and S.MetaDataID = M.ID and M.MultipleValues=1;

SetMetadataBlob INSERT INTO ServiceBlobMetaData (ServiceID, MetaDataID, MetaDataValue, BlobLength, EmbeddedFlag, DeleteFlag) VALUES (?,?,?,?,?,0);
SetMetadataKeyword INSERT INTO ServiceKeywordMetaData (ServiceID, MetaDataID, MetaDataValue, EmbeddedFlag, DeleteFlag) VALUES (?,?,?,?,0);
SetMetadataString INSERT INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataValue, EmbeddedFlag, DeleteFlag) VALUES (?,?,?,?,0);
SetMetadataNumeric INSERT INTO ServiceNumericMetaData (ServiceID, MetaDataID, MetaDataValue, EmbeddedFlag, DeleteFlag) VALUES (?,?,?,?,0);
SetMetadataDisplay INSERT INTO ServiceMetaDataDisplay (ServiceID, MetaDataID, MetaDataValue, EmbeddedFlag, DeleteFlag) VALUES (?,?,?,?,0);

DeleteMetadataBlob DELETE FROM ServiceBlobMetaData where ServiceID = ? and  MetaDataID=?;
DeleteMetadataKeyword DELETE FROM ServiceKeywordMetaData where ServiceID = ? and  MetaDataID=?;
DeleteMetadataString DELETE FROM ServiceMetaData where ServiceID = ? and  MetaDataID=?;
DeleteMetadataNumeric DELETE FROM ServiceNumericMetaData where ServiceID = ? and  MetaDataID=?;
DeleteMetadataDisplay DELETE FROM ServiceMetaDataDisplay where ServiceID = ? and  MetaDataID=?;

DeleteMetadataBlobValue DELETE FROM ServiceBlobMetaData where ServiceID = ? and  MetaDataID=? and MetaDataValue = ?;
DeleteMetadataKeywordValue DELETE FROM ServiceKeywordMetaData where ServiceID = ? and  MetaDataID=? and MetaDataValue = ?;
DeleteMetadataStringValue DELETE FROM ServiceMetaData where ServiceID = ? and  MetaDataID=? and MetaDataValue = ?;
DeleteMetadataNumericValue DELETE FROM ServiceNumericMetaData where ServiceID = ? and  MetaDataID=? and MetaDataValue = ?;

GetMetadataTypeInfo SELECT  ID, DataTypeID, MultipleValues, Weight  FROM MetaDataTypes where MetaName = ?;
GetMetadataTypes SELECT MetaName, ID, DataTypeID, MultipleValues,  Weight  FROM MetaDataTypes;
GetMetadataTypesLike SELECT MetaName, ID, DataTypeID, MultipleValues,  Weight  FROM MetaDataTypes WHERE MetaName glob ?;
GetWriteableMetadataTypes SELECT MetaName, ID, DataTypeID, MultipleValues,  Weight  FROM MetaDataTypes where writeable = 1;
GetWriteableMetadataTypesLike SELECT MetaName, ID, DataTypeID, MultipleValues,  Weight  FROM MetaDataTypes WHERE MetaName glob ? and  writeable = 1;

GetMetadataAliases SELECT distinct M.MetaName, M.ID from MetaDataTypes M, MetaDataChildren C where M.ID = C.ChildID and C.MetaDataID = ?; 
GetMetadataAliasesForName SELECT distinct M.MetaName, M.ID from MetaDataTypes M, MetaDataChildren C where M.ID = C.ChildID and C.MetaDataID = (select ID from MetaDataTypes where MetaName = ?) union select M.MetaName, M.ID from MetaDataTypes M where M.MetaName = ?; 

SelectMetadataClasses SELECT DISTINCT MetaName FROM MetaDataTypes;
InsertMetadataType INSERT INTO MetaDataTypes (MetaName, DataTypeID, MultipleValues, Weight) VALUES (?,?,?,?); 

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

InsertSearchResult1 insert into SearchResults1 (SID, Score) values (?,?);
DeleteSearchResults1 delete from SearchResults1;

GetWordID select WordID, WordCount from Words where Word =? and WordCount > 0;
DeleteWord delete from Words where WordID = ?;
InsertWord insert into Words (Word, WordCount) Values (?,1);
UpdateWordCount update Words set WordCount = ? where WordID = ?;
GetWordsTop select distinct WordID, Word from Words where WordCount > 0 order by WordCount asc limit ?;
GetWords select distinct W.WordID, W.Word from Words W where exists (select 1 from ServiceWords S where S.WordID = W.WordID);
GetWordCount select count(*) from Words W where WordCount > 0;

InsertServiceWord insert into  ServiceWords (WordID, ServiceID, ServiceType, score) values (?,?,?,?);
UpdateServiceWord update  ServiceWords  set score = ? where WordID = (select WordID from Words where Word = ?) and ServiceID = ?;
DeleteServiceWordForID delete from ServiceWords where ServiceID = ?;
DeleteServiceWords delete from ServiceWords where WordID = ?;
GetServiceWord select ServiceID, ServiceType, Score from ServiceWords where WordID = ? and serviceID > 0 and servicetype > 0 and score > 0;
ServiceCached select 1 from ServiceWords where ServiceID = ? and WordID = (select WordID from Words where Word = ?);
GetServiceWordCount select count(*) from ServiceWords where WordID = ?;

GetStats select 'Total files indexed', 	count(*) as n from Services where ServiceTypeID between 0 and 8 union select T.TypeName, count(*) as n from Services S, ServiceTypes T where S.ServiceTypeID = T.TypeID group by T.TypeName order by 2; 

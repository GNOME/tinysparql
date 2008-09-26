
GetMetaDataTypeID select ID From MetaDataTypes where MetaName = ?;

GetMetaDataType select DataTypeID From MetaDataTypes where MetaName = ?;

GetRelatedServiceIDs select TypeId from ServiceTypes where TypeName = ? or Parent = ?;

GetFileByID  SELECT  DISTINCT Path , Name, Mime, ServiceTypeID  FROM Services WHERE ID = ? and Enabled = 1;

GetFileByID2  SELECT DISTINCT (Path || '/' || Name) as uri, GetServiceName (ServiceTypeID), Mime FROM Services WHERE ID = ? and Enabled = 1;

GetFileByID3  SELECT  DISTINCT Path , Name, Mime, ServiceTypeID  FROM Services WHERE ID = ?;

GetEmailByID2  SELECT DISTINCT (S.Path || '/' || S.Name) as uri, 'Emails', S.Mime, M1.MetaDataDisplay, M2.MetaDataDisplay FROM Services S Left Outer Join ServiceMetaData M1 on S.ID = M1.ServiceID and M1.MetaDataID = (select ID From MetaDataTypes where MetaName ='Email:Subject') Left Outer Join ServiceMetaData M2 on S.ID = M2.ServiceID and M2.MetaDataID = (select ID From MetaDataTypes where MetaName ='Email:Sender') WHERE S.ID = ?;

GetEmailByID  SELECT DISTINCT (S.Path || '/' || S.Name) as uri, 'Emails', S.Mime, S.KeyMetadata1, S.KeyMetadata2, S.KeyMetadata3 FROM Services S  WHERE S.ID = ?;

GetApplicationByID  SELECT DISTINCT (S.Path || '/' || S.Name) as uri, 'Applications', 'Application', S.KeyMetadata1, S.KeyMetadata2, S.KeyMetadata3 FROM Services S  WHERE S.ID = ?;

GetFileMTime SELECT M.MetaDataValue  FROM Services F inner join ServiceNumericMetaData M on F.ID = M.ServiceID WHERE F.Path = ? and F.Name = ? and M.MetaDataID = (select ID From MetaDataTypes where MetaName ='File:Modified');

GetServices SELECT TypeName, Description, Parent  FROM ServiceTypes ORDER BY TypeID;
GetAllServices SELECT TypeID, TypeName, Parent, PropertyPrefix, Enabled, Embedded, HasMetadata, HasFullText, HasThumbs, ContentMetadata, Database, ShowServiceFiles, ShowServiceDirectories, KeyMetadata1, KeyMetadata2, KeyMetadata3, KeyMetadata4, KeyMetadata5, KeyMetadata6, KeyMetadata7, KeyMetadata8, KeyMetadata9, KeyMetadata10, KeyMetadata11  FROM ServiceTypes;

# GetNewID and UpdateNewID are deprecated !!
GetNewID SELECT OptionValue FROM Options WHERE OptionKey = 'Sequence';
UpdateNewID UPDATE Options set OptionValue = ? WHERE OptionKey = 'Sequence';
CreateEvent INSERT INTO Events (ServiceID, EventType) VALUES (?,?); 
GetEvents SELECT ID, ServiceID, EventType FROM Events WHERE BeingHandled = 1;
SetEventsBeingHandled UPDATE Events SET BeingHandled = 1;

GetLiveSearchAllIDs SELECT X.ServiceID FROM cache.LiveSearches AS X WHERE X.SearchID = ?
GetLiveSearchDeletedIDs SELECT E.ServiceID FROM Events as E, cache.LiveSearches as X WHERE E.ServiceID = X.ServiceID AND X.SearchID = ? AND E.EventType = 'Delete';
DeleteLiveSearchDeletedIDs DELETE FROM cache.LiveSearches AS Y WHERE Y.ServiceID IN SELECT ServiceID FROM Events as E, cache.LiveSearches as X WHERE E.ServiceID = X.ServiceID AND X.SearchID = ? AND E.EventType = 'Delete'

GetLiveSearchHitCount SELECT count(*) FROM cache.LiveSearches WHERE SearchID = ?;
LiveSearchStopSearch DELETE FROM cache.LiveSearches WHERE SearchID = ?

# GetNewEventID and UpdateNewEventID are deprecated !!
GetNewEventID SELECT OptionValue FROM Options WHERE OptionKey = 'EventSequence';
UpdateNewEventID UPDATE Options set OptionValue = ? WHERE OptionKey = 'EventSequence';

GetUpdateCount SELECT OptionValue FROM Options WHERE OptionKey = 'UpdateCount';
SetUpdateCount UPDATE Options set OptionValue = ?  WHERE OptionKey = 'UpdateCount';

GetOption SELECT OptionValue FROM Options WHERE OptionKey = ?;
SetOption REPLACE into Options (OptionKey, OptionValue) values (?,?);

CreateService INSERT INTO Services (ID, Path, Name, ServiceTypeID, Mime, Size, IsDirectory, IsLink, Offset, IndexTime, AuxilaryID) VALUES (?,?,?,?,?,?,?,?,?,?,?); 

MoveService UPDATE Services SET Path = ?, Name = ? WHERE Path = ? AND Name = ?;
MoveServiceChildren UPDATE Services SET Path = replace (Path, ?, ?);

GetServiceID SELECT ID, IndexTime, IsDirectory, ServiceTypeID FROM Services WHERE Path = ? AND Name = ?;

SelectFileChild SELECT ID, Path, Name, IsDirectory  FROM Services WHERE Path = ?;

SelectFileChildWithoutDirs SELECT Path, Name  FROM Services WHERE Path = ? and IsDirectory = 0;

SelectFileSubFolders SELECT ID, Path, Name, IsDirectory FROM Services WHERE (Path = ?  or Path glob ?) And IsDirectory = 1;

SelectSubFileIDs SELECT ID FROM Services WHERE (Path = ?  or Path glob ?);

UpdateFile UPDATE Services SET ServiceTypeID=?, Path=?, Name=?, Mime=?, Size=?, IndexTime =?, Offset=? WHERE ID = ?; 
UpdateFileMTime UPDATE Services SET IndexTime=? where Path = ? and Name = ?;
UpdateFileMove 	UPDATE Services SET Path = ?, Name = ? WHERE ID = ?;
UpdateFileMoveChild UPDATE Services SET Path = ? WHERE Path = ?; 

DeleteService1 Delete FROM Services Where (ID = ?);
DeleteService2 Delete FROM Services Where (Path = ?);
DeleteService3 Delete FROM Services Where (Path glob ?);
DeleteService4 Delete FROM BackupServices Where (Path = ?);
DeleteService5 Delete FROM BackupServices Where (Path glob ?);
DeleteService6 Delete FROM BackupServices Where (Path = ? and Name = ?);
DeleteService7 Delete FROM ServiceLinks Where (SourcePath = ? and SourceName = ?);
DeleteService8 Delete FROM ServiceLinks Where (SourcePath = ?) or (SourcePath glob ?);
DeleteService9 Delete FROM ServiceLinks Where (DestPath = ? and DestName = ?);
DeleteService10 Delete FROM ServiceLinks Where (DestPath = ?) or (DestPath glob ?); 
DeleteService11 DELETE FROM ServiceContents where ServiceID = ?;

DeleteServiceMetadata DELETE FROM ServiceMetaData WHERE ServiceID = ?;
DeleteServiceKeywordMetadata DELETE FROM ServiceMetaData WHERE ServiceID = ?;
DeleteServiceNumericMetadata DELETE FROM ServiceMetaData WHERE ServiceID = ?;

DeleteEmbeddedServiceMetadata1 DELETE FROM ServiceMetaData WHERE ServiceID = ? and MetaDataID in (select ID from MetaDataTypes where Embedded = 1);
DeleteEmbeddedServiceMetadata2 DELETE FROM ServiceKeywordMetaData WHERE ServiceID = ? and MetaDataID in (select ID from MetaDataTypes where Embedded = 1);
DeleteEmbeddedServiceMetadata3 DELETE FROM ServiceNumericMetaData WHERE ServiceID = ? and MetaDataID in (select ID from MetaDataTypes where Embedded = 1);

GetByServiceType SELECT  DISTINCT F.Path || '/' || F.Name as uri  FROM Services F WHERE F.ServiceTypeID in (select TypeId from ServiceTypes where TypeName = ? or Parent = ?) LIMIT ?,?;

SaveServiceContents REPLACE into ServiceContents (ServiceID, MetadataID, Content) values (?,?,compress (?));
DeleteContent DELETE FROM ServiceContents where ServiceID = ? and MetadataId = ?;
DeleteAllContents DELETE FROM ServiceContents where ServiceID = ?;
GetContents Select uncompress (Content) from ServiceContents where ServiceID = ? and MetadataID = ? and Content is not null;
GetFileContents Select substr(uncompress (Content), ?, ?) from ServiceContents where ServiceID = ?;
GetAllContents Select uncompress (Content) from ServiceContents where ServiceID = ? and Content is not null;

GetKeywordList Select distinct K.MetaDataValue, count(*) as totalcount from Services S, ServiceKeywordMetaData K where K.ServiceID = S.ID AND (S.ServiceTypeID in (select TypeId from ServiceTypes where TypeName = ? or Parent = ?)) AND  K.MetaDataId = 19 group by K.MetaDataValue order by totalcount desc, K.MetaDataValue asc;
GetKeywords Select MetaDataValue from ServiceKeywordMetaData where ServiceID = (select ID From Services where Path = ? and Name  = ?) and MetaDataId in (select ChildID from MetaDataChildren where MetadataId = (select ID from MetadataTypes T where MetaName = 'DC:Keywords'));

GetAllIndexable SELECT S.MetaDataValue as MetaValue, M.Weight, M.Filtered, M.Delimited  FROM ServiceMetaData S, MetaDataTypes M WHERE  S.MetaDataID = M.ID AND S.ServiceID = ? And S.MetaDataValue is not null and M.DatatypeID = 1 and M.Embedded >= ?;
GetAllIndexableKeywords SELECT K.MetaDataValue as MetaValue, M.Weight, M.Filtered, M.Delimited as MetaWeight FROM ServiceKeywordMetaData K, MetaDataTypes M WHERE  K.MetaDataID = M.ID AND K.ServiceID = ? And K.MetaDataValue is not null and M.DatatypeID = 0 and M.Embedded >= ?;

GetMetadataKeyword SELECT MetaDataValue FROM ServiceKeywordMetaData WHERE ServiceID = ? AND MetaDataID = ?;
GetMetadata SELECT MetaDataDisplay FROM ServiceMetaData WHERE ServiceID = ? AND MetaDataID = ?;
GetMetadataNumeric SELECT MetaDataValue FROM ServiceNumericMetaData WHERE ServiceID = ? AND MetaDataID = ?;

GetMetadataIDValueKeyword SELECT MetadataID, MetadataValue FROM ServiceKeywordMetadata WHERE ServiceID = ? ORDER BY MetadataID
GetMetadataIDValue SELECT MetadataID, MetadataValue FROM ServiceMetadata WHERE ServiceID = ? ORDER BY MetadataID
GetMetadataIDValueNumeric SELECT MetadataID, MetadataValue FROM ServiceNumericMetadata WHERE ServiceID = ? ORDER BY MetadataID

SetMetadataKeyword INSERT INTO ServiceKeywordMetaData (ServiceID, MetaDataID, MetaDataValue) VALUES (?,?,?);
SetMetadata INSERT INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataValue, MetaDataDisplay) VALUES (?,?,?,?);
SetMetadataNumeric INSERT INTO ServiceNumericMetaData (ServiceID, MetaDataID, MetaDataValue) VALUES (?,?,?);

DeleteMetadataKeyword DELETE FROM ServiceKeywordMetaData where ServiceID = ? and  MetaDataID=?;
DeleteMetadata DELETE FROM ServiceMetaData where ServiceID = ? and  MetaDataID=?;
DeleteMetadataNumeric DELETE FROM ServiceNumericMetaData where ServiceID = ? and  MetaDataID=?;

DeleteMetadataKeywordValue DELETE FROM ServiceKeywordMetaData where ServiceID = ? and  MetaDataID=? and MetaDataValue = ?;
DeleteMetadataValue DELETE FROM ServiceMetaData where ServiceID = ? and  MetaDataID=? and MetaDataDisplay=?;
DeleteMetadataNumericValue DELETE FROM ServiceNumericMetaData where ServiceID = ? and  MetaDataID=? and MetaDataValue=?;

GetMetadataTypeInfo Select ID, MetaName, DataTypeID, DisplayName, Description, Enabled, UIVisible, FieldName, Weight, Embedded, MultipleValues, Delimited, Filtered, Abstract FROM MetaDataTypes where MetaName = ?;
GetMetadataTypes SELECT ID, MetaName, DataTypeID, FieldName, Weight, Embedded, MultipleValues, Delimited, Filtered, Abstract FROM MetaDataTypes;
GetMetadataTypeDetails SELECT ID, MetaName, DataTypeID, DisplayName, Description, Enabled, UIVisible, FieldName, Weight, Embedded, MultipleValues, Delimited, Filtered, Abstract FROM MetaDataTypes;
GetMetadataTypesLike SELECT ID, MetaName, DataTypeID, DisplayName, Description, Enabled, UIVisible, FieldName, Weight, Embedded, MultipleValues, Delimited, Filtered, Abstract FROM MetaDataTypes WHERE MetaName glob ?;
GetWriteableMetadataTypes SELECT ID, MetaName, DataTypeID, DisplayName, Description, Enabled, UIVisible, FieldName, Weight, Embedded, MultipleValues, Delimited, Filtered, Abstract FROM MetaDataTypes where Embedded = 0;
GetWriteableMetadataTypesLike SELECT ID, MetaName, DataTypeID, DisplayName, Description, Enabled, UIVisible, FieldName, Weight, Embedded, MultipleValues, Delimited, Filtered, Abstract FROM MetaDataTypes WHERE MetaName glob ? and  Embedded = 0;

InsertMetaDataChildren INSERT INTO  MetaDataChildren (ChildID,MetadataID) VALUES (?,(select ID from MetaDataTypes where MetaName = ?));
GetMetadataAliases SELECT distinct M.MetaName, M.ID from MetaDataTypes M, MetaDataChildren C where M.ID = C.ChildID and C.MetaDataID = ?; 
GetMetadataAliasesForName SELECT distinct M.MetaName, M.ID from MetaDataTypes M, MetaDataChildren C where M.ID = C.ChildID and C.MetaDataID = (select ID from MetaDataTypes where MetaName = ?) union select M.MetaName, M.ID from MetaDataTypes M where M.MetaName = ?; 

SelectRegisteredClasses SELECT DISTINCT TypeName FROM ServiceTypes;

InsertMetadataType INSERT INTO MetaDataTypes (MetaName) Values (?);

InsertServiceType REPLACE INTO  ServiceTypes (TypeName) Values (?);
InsertServiceTileMetadata REPLACE Into ServiceTileMetadata (ServiceTypeID, MetaName) VALUES (?, ?);
InsertServiceTabularMetadata REPLACE Into ServiceTabularMetadata (ServiceTypeID, MetaName) VALUES (?, ?);

GetServiceTypes select TypeID, TypeName, Parent, Enabled, Embedded, HasMetadata, HasFullText, HasThumbs, ContentMetadata, KeyMetadata1,  KeyMetadata2, KeyMetadata3, KeyMetadata4, KeyMetadata5, KeyMetadata6, KeyMetadata7, KeyMetadata8, KeyMetadata9, KeyMetadata10, KeyMetadata11 From ServiceTypes;
GetServiceTypeDetails select TypeID, TypeName, DisplayName, Parent, Enabled, Embedded, ChildResource, CreateDesktopFile, CanCopy, CanDelete, HasMetadata, HasFullText, HasThumbs, ContentMetadata, UIView, Description, Database, Icon, IndexerExec, ThumbExec, ViewerExec, UIVisible, UIMetadata1, UIMetadata2, UIMetadata3, KeyMetadata1, KeyMetadata2, KeyMetadata3, KeyMetadata4, KeyMetadata5, KeyMetadata6 FROM ServiceTypes;
GetServiceTypeDetailsByName select TypeID, TypeName, DisplayName, Parent, Enabled, Embedded, ChildResource, CreateDesktopFile, CanCopy, CanDelete, HasMetadata, HasFullText, HasThumbs, ContentMetadata, UIView, Description, Database, Icon, IndexerExec, ThumbExec, ViewerExec, UIVisible, UIMetadata1, UIMetadata2, UIMetadata3, KeyMetadata1, KeyMetadata2, KeyMetadata3, KeyMetadata4, KeyMetadata5, KeyMetadata6 FROM ServiceTypes where TypeName = ?;
GetServiceTypeDetailsByID select TypeId, TypeName, DisplayName, Parent, Enabled, Embedded, ChildResource, CreateDesktopFile, CanCopy, CanDelete, HasMetadata, HasFullText, HasThumbs, ContentMetadata, UIView, Description, Database, Icon, IndexerExec, ThumbExec, ViewerExec, UIVisible, UIMetadata1, UIMetadata2, UIMetadata3, KeyMetadata1, KeyMetadata2, KeyMetadata3, KeyMetadata4, KeyMetadata5, KeyMetadata6 FROM ServiceTypes where TypeID = ?;
GetServiceTile	select M.MetaName, M.ID from MetaDataTypes M where M.ID in (select MetadataTypeID  from ServiceTileMetadata where ServiceTypeID = ?);
GetServiceTable select M.MetaName, M.ID from MetaDataTypes M where M.ID in (select MetadataTypeID  from ServiceTabularMetadata where ServiceTypeID = ?);

InsertMimes replace into FileMimes (Mime) Values (?);
InsertMimePrefixes replace into FileMimePrefixes (MimePrefix) Values (?);

GetMimeForServiceId select Mime from FileMimes where ServiceTypeId = ?;
GetMimePrefixForServiceId select MimePrefix from FileMimePrefixes where ServiceTypeId = ?;

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

GetMboxes  select MailApp, MailType, Filename, Path, UriPrefix, Offset, LastOffset, MailCount, JunkCount, DeleteCount, Mtime  from MailSummary;
GetMBoxDetails select MailApp, MailType, Filename, UriPrefix, Offset, LastOffset, MailCount, JunkCount, DeleteCount, Mtime  from MailSummary where path = ?;
GetMboxID select ID from MailSummary where path = ?;
GetMBoxPath select path from MailSummary where FileName = ?;
InsertMboxDetails insert into MailSummary (MailApp, MailType, FileName, Path, UriPrefix, Offset, LastOffset, MailCount, JunkCount, DeleteCount, Mtime) values (?,?,?,?,?,0,0,0,0,0,0);
UpdateMboxOffset update MailSummary set Offset = ? where Path = ?;
UpdateMboxCounts update MailSummary set MailCount = ?, JunkCount = ?, DeleteCount = ? where Path = ?;

SetJunkMbox update MailSummary set NeedsChecking = ? where path = ?;

LookupJunk select SummaryID from JunkMail where UID = ? and SummaryID = ?;
InsertJunk insert into JunkMail values (?,?);

selectStats Select Sum(TypeCount) from ServiceTypes where TypeName = ? or TypeName in (select TypeName from ServiceTypes where Parent = ?);
IncStat UPDATE ServiceTypes set TypeCount = (TypeCount + 1) where TypeName = ?;
DecStat UPDATE ServiceTypes set TypeCount = (TypeCount - 1) where TypeName = ?;
GetStats Select TypeName, TypeCount from ServiceTypes Group By TypeName order by TypeID asc;

InsertBackupService INSERT INTO BackupServices (Path, Name) select S.Path, S.Name from Services S where S.ID = ?;
UpdateBackupService Update BackupServices set Path = ? and Name = ? where Path = ? and Name = ?;
GetBackupService Select ID from BackupServices where Path = ? and  Name = ?;
GetBackupServiceByID Select B.ID from BackupServices B, Services S where B.Path = S.Path and  B.Name = S.Name and S.ID = ?;
GetBackupMetadata select M.MetaName, B.UserValue from BackupMetadata B, MetadataTypes M, BackupServices BS where B.MetadataID = M.ID and B.ServiceID = BS.ID and BS.Path = ? and BS.Name = ?;
SetBackupMetadata INSERT INTO BackupMetadata (ServiceID, MetadataID, UserValue) VALUES (?,?,?);
DeleteBackupMetadataValue Delete From BackupMetadata where  ServiceID = ? and MetadataID = ? and UserValue = ?;
DeleteBackupMetadata Delete From BackupMetadata where  ServiceID = ? and MetadataID = ?;

GetWords Select word from HitIndex where word glob ?;
GetWordBatch Select ROWID, word, HitCount, HitArraySize from HitIndex limit 100;
GetHitDetails Select ROWID, HitCount, HitArraySize From HitIndex where word = ?;
DeleteWord delete from HitIndex where ROWID = ?;
InsertHitDetails Insert into HitIndex (Word, HitCount, HitArraySize, HitArray) Values (?,?,?, ZeroBlob(?));
UpdateHitDetails Update HitIndex set HitCount = ?, HitArraySize = ? where ROWID = ?;
ResizeHitDetails Update HitIndex set HitCount = ?, HitArraySize = ?, HitArray = Zeroblob(?) where ROWID = ?;

InsertXesamMetadataType INSERT INTO XesamMetaDataTypes (MetaName) Values (?);
InsertXesamServiceType INSERT INTO XesamServiceTypes (TypeName) Values (?);
InsertXesamMetaDataMapping INSERT INTO XesamMetaDataMapping (XesamMetaName, MetaName) Values (?, ?);
InsertXesamServiceMapping INSERT INTO XesamServiceMapping (XesamTypeName, TypeName) Values (?, ?);
InsertXesamServiceLookup REPLACE INTO XesamServiceLookup (XesamTypeName, TypeName) Values (?, ?);
InsertXesamMetaDataLookup REPLACE INTO XesamMetaDataLookup (XesamMetaName, MetaName) Values (?, ?);

GetXesamServiceParents SELECT TypeName, Parents FROM XesamServiceTypes;
GetXesamServiceChildren SELECT Child FROM XesamServiceChildren WHERE Parent = ?;
GetXesamServiceMappings SELECT TypeName FROM XesamServiceMapping WHERE XesamTypeName = ?;
GetXesamServiceLookups SELECT DISTINCT TypeName FROM XesamServiceLookup WHERE XesamTypeName = ?;

GetXesamMetaDataParents SELECT MetaName, Parents FROM XesamMetaDataTypes;
GetXesamMetaDataChildren SELECT Child FROM XesamMetaDataChildren WHERE Parent = ?;
GetXesamMetaDataMappings SELECT MetaName FROM XesamMetaDataMapping WHERE XesamMetaName = ?;
GetXesamMetaDataLookups SELECT DISTINCT MetaName FROM XesamMetaDataLookup WHERE XesamMetaName = ?;
GetXesamMetaDataTextLookups SELECT DISTINCT L.MetaName FROM XesamMetaDataLookup L INNER JOIN XesamMetaDataTypes T ON (T.MetaName = L.XesamMetaName) WHERE T.DataTypeID = 3;

GetXesamMetaDataTypes SELECT ID, MetaName, DataTypeID, FieldName, Weight, Embedded, MultipleValues, Delimited, Filtered, Abstract FROM XesamMetaDataTypes;
GetXesamServiceTypes SELECT TypeID, TypeName, Parents, PropertyPrefix, Enabled, Embedded, HasMetadata, HasFullText, HasThumbs, ContentMetadata, Database, ShowServiceFiles, ShowServiceDirectories, KeyMetadata1, KeyMetadata2, KeyMetadata3, KeyMetadata4, KeyMetadata5, KeyMetadata6, KeyMetadata7, KeyMetadata8, KeyMetadata9, KeyMetadata10, KeyMetadata11  FROM XesamServiceTypes;

InsertXesamMimes replace into XesamFileMimes (Mime) Values (?);
InsertXesamMimePrefixes replace into XesamFileMimePrefixes (MimePrefix) Values (?);

GetXesamMimeForServiceId select Mime from XesamFileMimes where ServiceTypeId = ?;
GetXesamMimePrefixForServiceId select MimePrefix from XesamFileMimePrefixes where ServiceTypeId = ?;

/*
 * This file is dedicated to stored procedures 
 */


/*
 * Metadata queries
 */
GetApplicationByID             SELECT (S.Path || '/' || S.Name) AS uri, 'Applications', 'Application', S.KeyMetadata1, S.KeyMetadata2, S.KeyMetadata3 FROM Services AS S WHERE S.ID = ?;
GetAllServices                 SELECT TypeID, TypeName, Parent, PropertyPrefix, Enabled, Embedded, HasMetadata, HasFullText, HasThumbs, ContentMetadata, Database, ShowServiceFiles, ShowServiceDirectories, KeyMetadata1, KeyMetadata2, KeyMetadata3, KeyMetadata4, KeyMetadata5, KeyMetadata6, KeyMetadata7, KeyMetadata8, KeyMetadata9, KeyMetadata10, KeyMetadata11 FROM ServiceTypes;
GetEmailByID                   SELECT (S.Path || '/' || S.Name) AS uri, 'Emails', S.Mime, S.KeyMetadata1, S.KeyMetadata2, S.KeyMetadata3 FROM Services AS S WHERE S.ID = ?;
GetFileByID                    SELECT S.Path, S.Name, S.Mime, S.ServiceTypeID FROM Services AS S WHERE S.ID = ? AND S.Enabled = 1 AND (S.AuxilaryID = 0 OR S.AuxilaryID IN (SELECT VolumeID FROM Volumes WHERE Enabled = 1));
GetFileByID2                   SELECT (S.Path || '/' || S.Name) AS uri, GetServiceName (ServiceTypeID), S.Mime FROM Services AS S WHERE S.ID = ? AND S.Enabled = 1 AND (S.AuxilaryID = 0 OR S.AuxilaryID IN (SELECT VolumeID FROM Volumes WHERE Enabled = 1));
GetFileMTime                   SELECT M.MetaDataValue FROM Services AS S INNER JOIN ServiceNumericMetaData M ON S.ID = M.ServiceID WHERE S.Path = ? AND S.Name = ? AND M.MetaDataID = (SELECT ID FROM MetaDataTypes WHERE MetaName ='File:Modified');
GetServices                    SELECT TypeName, Description, Parent FROM ServiceTypes ORDER BY TypeID;
GetFileChildren                SELECT ID, Name FROM Services WHERE Path = ?;

/*
 * Live search queries
 */
CreateEvent                    INSERT INTO Events (ServiceID, EventType) VALUES (?,?); 

GetLiveSearchAllIDs            SELECT X.ServiceID FROM cache.LiveSearches AS X WHERE X.SearchID = ?
GetLiveSearchDeletedIDs        SELECT E.ServiceID FROM Events AS E, cache.LiveSearches AS X WHERE E.ServiceID = X.ServiceID AND X.SearchID = ? AND E.EventType = 'Delete';
GetLiveSearchHitCount          SELECT count(*) FROM cache.LiveSearches WHERE SearchID = ?;

DeleteLiveSearchDeletedIDs     DELETE FROM cache.LiveSearches AS Y WHERE Y.ServiceID IN SELECT ServiceID FROM Events AS E, cache.LiveSearches AS X WHERE E.ServiceID = X.ServiceID AND X.SearchID = ? AND E.EventType = 'Delete'

LiveSearchStopSearch           DELETE FROM cache.LiveSearches WHERE SearchID = ?

/*
 * Option queries
 */
GetOption                      SELECT OptionValue FROM Options WHERE OptionKey = ?;
SetOption                      REPLACE INTO Options (OptionKey, OptionValue) VALUES (?,?);

/*
 * File queries
 */
CreateService                  INSERT INTO Services (ID, Path, Name, ServiceTypeID, Mime, Size, IsDirectory, IsLink, Offset, IndexTime, AuxilaryID) VALUES (?,?,?,?,?,?,?,?,?,?,?); 

MoveService                    UPDATE Services SET Path = ?, Name = ? WHERE Path = ? AND Name = ?;
MoveServiceChildren            UPDATE Services SET Path = replace (Path, ?, ?) WHERE Path = ? OR Path LIKE (? || '/%');

SelectFileChild                SELECT ID, Path, Name, IsDirectory FROM Services WHERE Path = ?;

DeleteContent                  DELETE FROM ServiceContents WHERE ServiceID = ? AND MetadataId = ?;
DeleteService1                 DELETE FROM Services WHERE ID = ?;
DeleteServiceRecursively       DELETE FROM Services WHERE Path = ? OR Path LIKE (? || '/%');
DeleteServiceMetadata          DELETE FROM ServiceMetaData WHERE ServiceID = ?;
DeleteServiceKeywordMetadata   DELETE FROM ServiceMetaData WHERE ServiceID = ?;
DeleteServiceNumericMetadata   DELETE FROM ServiceMetaData WHERE ServiceID = ?;

GetServiceID                   SELECT ID, IndexTime, IsDirectory, ServiceTypeID FROM Services WHERE Path = ? AND Name = ?;
GetByServiceType               SELECT DISTINCT S.Path || '/' || S.Name AS uri FROM Services AS S WHERE S.Enabled = 1 AND (S.AuxilaryID = 0 OR S.AuxilaryID IN (SELECT VolumeID FROM Volumes WHERE Enabled = 1)) AND S.ServiceTypeID IN (SELECT TypeId FROM ServiceTypes WHERE TypeName = ? OR Parent = ?) LIMIT ?,?;
GetContents                    SELECT uncompress (Content) FROM ServiceContents WHERE ServiceID = ? AND MetadataID = ? AND Content is not null;
GetFileContents                SELECT substr(uncompress (Content), ?, ?) FROM ServiceContents WHERE ServiceID = ?;
GetAllContents                 SELECT uncompress (Content) FROM ServiceContents WHERE ServiceID = ? AND Content is not null;
GetKeywordList                 SELECT DISTINCT K.MetaDataValue, count(*) AS totalcount FROM Services AS S, ServiceKeywordMetaData K WHERE K.ServiceID = S.ID AND (S.ServiceTypeID IN (SELECT TypeId FROM ServiceTypes WHERE TypeName = ? OR Parent = ?)) AND K.MetaDataId = 19 GROUP BY K.MetaDataValue ORDER BY totalcount desc, K.MetaDataValue ASC;

SaveServiceContents            REPLACE INTO ServiceContents (ServiceID, MetadataID, Content) VALUES (?,?,compress (?));

/*
 * Metadata/MIME queries
 */
GetUserMetadataBackup          SELECT S.Path || '/' || S.Name, T.TypeName, M.MetadataID, M.MetadataDisplay From Services S, ServiceMetadata M, ServiceTypes T WHERE (S.ID == M.ServiceID) AND (S.ServiceTypeID == T.TypeID) AND (M.MetadataID IN (SELECT ID From MetadataTypes WHERE Embedded = 0)) UNION SELECT S.Path || '/' || S.Name, T.TypeName, M.MetadataID, upper(M.MetadataValue) From Services S, ServiceNumericMetadata M, ServiceTypes T WHERE (S.ID == M.ServiceID) AND (S.ServiceTypeID == T.TypeID) AND (M.MetadataID IN (SELECT ID From MetadataTypes WHERE Embedded = 0)) UNION SELECT S.Path || '/' || S.Name, T.Typename, M.MetadataID, M.MetadataValue From Services S, ServiceKeywordMetadata M, ServiceTypes T WHERE (S.ID == M.ServiceID) AND (S.ServiceTypeID == T.TypeID) AND (M.MetadataID IN (SELECT ID From MetadataTypes WHERE Embedded = 0));
GetAllMetadata                 SELECT MetadataID, MetadataDisplay FROM ServiceMetadata WHERE ServiceID = ? UNION SELECT MetadataID, MetadataValue FROM ServiceKeywordMetadata WHERE ServiceID = ? UNION SELECT MetadataID, upper(MetadataValue) FROM ServiceNumericMetadata WHERE ServiceID = ?;
GetMetadata                    SELECT MetaDataDisplay FROM ServiceMetaData WHERE ServiceID = ? AND MetaDataID = ?;
GetMetadataAliases             SELECT DISTINCT M.MetaName, M.ID FROM MetaDataTypes AS M, MetaDataChildren AS C WHERE M.ID = C.ChildID AND C.MetaDataID = ?; 
GetMetadataAliasesForName      SELECT DISTINCT M.MetaName, M.ID FROM MetaDataTypes AS M, MetaDataChildren AS C WHERE M.ID = C.ChildID AND C.MetaDataID = (SELECT ID FROM MetaDataTypes WHERE MetaName = ?) UNION SELECT M.MetaName, M.ID FROM MetaDataTypes AS M WHERE M.MetaName = ?; 
GetMetadataKeyword             SELECT MetaDataValue FROM ServiceKeywordMetaData WHERE ServiceID = ? AND MetaDataID = ?;
GetMetadataNumeric             SELECT MetaDataValue FROM ServiceNumericMetaData WHERE ServiceID = ? AND MetaDataID = ?;
GetMetadataTypes               SELECT ID, MetaName, DataTypeID, FieldName, Weight, Embedded, MultipleValues, Delimited, Filtered, Abstract FROM MetaDataTypes;

SetMetadata                    INSERT INTO ServiceMetaData (ServiceID, MetaDataID, MetaDataValue, MetaDataDisplay) VALUES (?,?,?,?);
SetMetadataKeyword             INSERT INTO ServiceKeywordMetaData (ServiceID, MetaDataID, MetaDataValue) VALUES (?,?,?);
SetMetadataNumeric             INSERT INTO ServiceNumericMetaData (ServiceID, MetaDataID, MetaDataValue) VALUES (?,?,?);

DeleteMetadata                 DELETE FROM ServiceMetaData WHERE ServiceID = ? AND MetaDataID = ?;
DeleteMetadataKeyword          DELETE FROM ServiceKeywordMetaData WHERE ServiceID = ? AND MetaDataID = ?;
DeleteMetadataKeywordValue     DELETE FROM ServiceKeywordMetaData WHERE ServiceID = ? AND MetaDataID = ? AND MetaDataValue = ?;
DeleteMetadataValue            DELETE FROM ServiceMetaData WHERE ServiceID = ? AND MetaDataID = ? AND MetaDataDisplay = ?;
DeleteMetadataNumeric          DELETE FROM ServiceNumericMetaData WHERE ServiceID = ? AND MetaDataID = ?;

InsertMetaDataChildren         INSERT INTO MetaDataChildren (ChildID,MetadataID) VALUES (?,(SELECT ID FROM MetaDataTypes WHERE MetaName = ?));
InsertMetadataType             INSERT INTO MetaDataTypes (MetaName) VALUES (?);
InsertMimePrefixes             REPLACE INTO FileMimePrefixes (MimePrefix) VALUES (?);
InsertMimes                    REPLACE INTO FileMimes (Mime) VALUES (?);
InsertServiceTabularMetadata   REPLACE INTO ServiceTabularMetadata (ServiceTypeID, MetaName) VALUES (?, ?);
InsertServiceTileMetadata      REPLACE INTO ServiceTileMetadata (ServiceTypeID, MetaName) VALUES (?, ?);
InsertServiceType              REPLACE INTO ServiceTypes (TypeName) VALUES (?);

GetMimeForServiceId            SELECT Mime FROM FileMimes WHERE ServiceTypeId = ?;
GetMimePrefixForServiceId      SELECT MimePrefix FROM FileMimePrefixes WHERE ServiceTypeId = ?;

/* 
 * Pending files queries - currently unused 
 */
ExistsPendingFiles             SELECT count (*) FROM FilePending WHERE Action <> 20;
InsertPendingFile              INSERT INTO FilePending (FileID, Action, PendingDate, FileUri, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID) VALUES (?,?,?,?,?,?,?,?,?,?);
CountPendingMetadataFiles      SELECT count (*) FROM FilePending WHERE Action = 20;
SelectPendingByUri             SELECT FileID, FileUri, Action, MimeType, IsDir, IsNew, RefreshEmbedded, RefreshContents, ServiceTypeID FROM FilePending WHERE FileUri = ?;
UpdatePendingFile              UPDATE FilePending SET PendingDate = ?, Action = ? WHERE FileUri = ?;
DeletePendingFile              DELETE FROM FilePending WHERE FileUri = ?;

/* 
 * Watch queries - currently unused
 */
GetWatchUri                    SELECT URI FROM FileWatches WHERE WatchID = ?;
GetWatchID                     SELECT WatchID FROM FileWatches WHERE URI = ?;
GetSubWatches                  SELECT WatchID FROM FileWatches WHERE URI glob ?;
DeleteWatch                    DELETE FROM FileWatches WHERE URI = ?;
DeleteSubWatches               DELETE FROM FileWatches WHERE URI glob ?;
InsertWatch                    INSERT INTO FileWatches (URI, WatchID) VALUES (?,?);

/*
 * Search result queries
 */
InsertSearchResult1            INSERT INTO SearchResults1 (SID, Score) VALUES (?,?);
DeleteSearchResults1           DELETE FROM SearchResults1;

/*
 * Statistics queries
 */
IncStat                        UPDATE ServiceTypes SET TypeCount = (TypeCount + 1) WHERE TypeName = ?;
DecStat                        UPDATE ServiceTypes SET TypeCount = (TypeCount - 1) WHERE TypeName = ?;
GetStats                       SELECT TypeName, TypeCount FROM ServiceTypes GROUP BY TypeName ORDER BY TypeID ASC;

GetHitDetails                  SELECT ROWID, HitCount, HitArraySize FROM HitIndex WHERE word = ?;

/*
 * Volume handling
 */

GetVolumeID                    SELECT VolumeID FROM Volumes WHERE UDI = ?;
GetVolumeByPath                SELECT VolumeID FROM Volumes WHERE Enabled = 1 AND (? = MountPath OR ? LIKE (MountPath || '/%'));
GetVolumesToClean              SELECT VolumeID FROM Volumes WHERE DisabledDate < date('now', '-3 day');
InsertVolume                   INSERT INTO Volumes (MountPath, UDI, Enabled, DisabledDate) VALUES (?, ?, 1, date('now'));
EnableVolume                   UPDATE Volumes SET MountPath = ?, Enabled = 1 WHERE UDI = ?;
DisableVolume                  UPDATE Volumes SET Enabled = 0, DisabledDate = date ('now') WHERE UDI = ?;
DisableAllVolumes              UPDATE Volumes SET Enabled = 0;

/*
 * XESAM queries
 */
InsertXesamMetadataType        INSERT INTO XesamMetaDataTypes (MetaName) VALUES (?);
InsertXesamServiceType         INSERT INTO XesamServiceTypes (TypeName) VALUES (?);
InsertXesamMetaDataMapping     INSERT INTO XesamMetaDataMapping (XesamMetaName, MetaName) VALUES (?, ?);
InsertXesamServiceMapping      INSERT INTO XesamServiceMapping (XesamTypeName, TypeName) VALUES (?, ?);
InsertXesamServiceLookup       REPLACE INTO XesamServiceLookup (XesamTypeName, TypeName) VALUES (?, ?);
InsertXesamMetaDataLookup      REPLACE INTO XesamMetaDataLookup (XesamMetaName, MetaName) VALUES (?, ?);

GetXesamServiceParents         SELECT TypeName, Parents FROM XesamServiceTypes;
GetXesamServiceChildren        SELECT Child FROM XesamServiceChildren WHERE Parent = ?;
GetXesamServiceMappings        SELECT TypeName FROM XesamServiceMapping WHERE XesamTypeName = ?;
GetXesamServiceLookups         SELECT DISTINCT TypeName FROM XesamServiceLookup WHERE XesamTypeName = ?;

GetXesamMetaDataParents        SELECT MetaName, Parents FROM XesamMetaDataTypes;
GetXesamMetaDataChildren       SELECT Child FROM XesamMetaDataChildren WHERE Parent = ?;
GetXesamMetaDataMappings       SELECT MetaName FROM XesamMetaDataMapping WHERE XesamMetaName = ?;
GetXesamMetaDataLookups        SELECT DISTINCT MetaName FROM XesamMetaDataLookup WHERE XesamMetaName = ?;
GetXesamMetaDataTextLookups    SELECT DISTINCT L.MetaName FROM XesamMetaDataLookup AS L INNER JOIN XesamMetaDataTypes AS T ON (T.MetaName = L.XesamMetaName) WHERE T.DataTypeID = 3;

GetXesamMetaDataTypes          SELECT ID, MetaName, DataTypeID, FieldName, Weight, Embedded, MultipleValues, Delimited, Filtered, Abstract FROM XesamMetaDataTypes;
GetXesamServiceTypes           SELECT TypeID, TypeName, Parents, PropertyPrefix, Enabled, Embedded, HasMetadata, HasFullText, HasThumbs, ContentMetadata, Database, ShowServiceFiles, ShowServiceDirectories, KeyMetadata1, KeyMetadata2, KeyMetadata3, KeyMetadata4, KeyMetadata5, KeyMetadata6, KeyMetadata7, KeyMetadata8, KeyMetadata9, KeyMetadata10, KeyMetadata11 FROM XesamServiceTypes;

InsertXesamMimes               REPLACE INTO XesamFileMimes (Mime) VALUES (?);
InsertXesamMimePrefixes        REPLACE INTO XesamFileMimePrefixes (MimePrefix) VALUES (?);

GetXesamMimeForServiceId       SELECT Mime FROM XesamFileMimes WHERE ServiceTypeId = ?;
GetXesamMimePrefixForServiceId SELECT MimePrefix FROM XesamFileMimePrefixes WHERE ServiceTypeId = ?;

/*
 * Turtle importing
 */
DeleteServiceAll               DELETE FROM Services WHERE ServiceTypeID = ?;

/*
 * Deprecated
 */
GetNewID                       SELECT OptionValue FROM Options WHERE OptionKey = 'Sequence';
UpdateNewID                    UPDATE Options SET OptionValue = ? WHERE OptionKey = 'Sequence';

GetNewEventID                  SELECT OptionValue FROM Options WHERE OptionKey = 'EventSequence';
UpdateNewEventID               UPDATE Options SET OptionValue = ? WHERE OptionKey = 'EventSequence';

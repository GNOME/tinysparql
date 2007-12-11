CreateService% INSERT INTO %.Services (ID, Path, Name, ServiceTypeID, Mime, Size, IsDirectory, IsLink, Offset, IndexTime, AuxilaryID) VALUES (?,?,?,?,?,?,?,?,?,?,?); 

GetServiceID% SELECT ID, IndexTime, IsDirectory, ServiceTypeID FROM %.Services WHERE Path = ? AND Name = ?;

DeleteService1% Delete FROM %.Services Where (ID = ?);
DeleteService2% Delete FROM %.Services Where (Path = ?);
DeleteService3% Delete FROM %.Services Where (Path glob ?);

GetByServiceType% SELECT  DISTINCT F.Path || '/' || F.Name as uri  FROM %.Services F WHERE F.ServiceTypeID in (select TypeId from ServiceTypes where TypeID = ? or Parent = ?) LIMIT ?,?;

GetKeywordList% Select distinct K.MetaDataValue, count(*) as totalcount from %.Services S, %.ServiceKeywordMetadata K where K.ServiceID = S.ID AND (S.ServiceTypeID in (select TypeId from ServiceTypes where TypeID = ? or Parent = ?)) and K.MetaDataId in (select ChildID from MetaDataChildren where MetadataId = (select ID from MetadataTypes T where MetaName = 'DC:Keywords')) group by K.MetaDataValue order by totalcount desc, K.MetaDataValue asc;
GetKeywords% Select MetaDataValue from %.ServiceKeywordMetadata where ServiceID = (select ID From %.Services where Path = ? and Name  = ?) and MetaDataId in (select ChildID from MetaDataChildren where MetadataId = (select ID from MetadataTypes T where MetaName = 'DC:Keywords'));

GetAllIndexable% SELECT S.MetaDataValue as MetaValue, M.Weight, M.Filtered, M.Delimited  FROM %.ServiceMetadata S, MetaDataTypes M WHERE  S.MetaDataID = M.ID AND S.ServiceID = ? And S.MetaDataValue is not null and M.DatatypeID = 1 and M.Embedded >= ?;
GetAllIndexableKeywords% SELECT K.MetaDataValue as MetaValue, M.Weight, M.Filtered, M.Delimited as MetaWeight FROM %.ServiceKeywordMetadata K, MetaDataTypes M WHERE  K.MetaDataID = M.ID AND K.ServiceID = ? And K.MetaDataValue is not null and M.DatatypeID = 0 and M.Embedded >= ?;

GetMetadataKeyword% SELECT MetaDataValue FROM %.ServiceKeywordMetadata WHERE ServiceID = ? AND MetaDataID = ?;
GetMetadataString% SELECT MetaDataDisplay, MetaDataValue FROM %.ServiceMetadata WHERE ServiceID = ? AND MetaDataID = ?;

SetMetadataKeyword% INSERT INTO %.ServiceKeywordMetadata (ServiceID, MetaDataID, MetaDataValue) VALUES (?,?,?);
SetMetadata% INSERT INTO %.ServiceMetadata (ServiceID, MetaDataID, MetaDataValue, MetaDataDisplay) VALUES (?,?,?,?);

DeleteMetadataKeyword% DELETE FROM %.ServiceKeywordMetadata where ServiceID = ? and  MetaDataID=?;
DeleteMetadata% DELETE FROM %.ServiceMetadata where ServiceID = ? and  MetaDataID=?;

DeleteMetadataKeywordValue% DELETE FROM %.ServiceKeywordMetadata where ServiceID = ? and  MetaDataID=? and MetaDataValue = ?;
DeleteMetadataValue% DELETE FROM %.ServiceMetadata where ServiceID = ? and  MetaDataID=? and MetaDataValue=?;

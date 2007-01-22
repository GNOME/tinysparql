
CREATE TABLE Options (	
	OptionKey 	Text COLLATE NOCASE not null,	
	OptionValue	Text COLLATE NOCASE
);

insert Into Options (OptionKey, OptionValue) values ('DBVersion', '15');
insert Into Options (OptionKey, OptionValue) values ('Sequence', '0');
insert Into Options (OptionKey, OptionValue) values ('UpdateCount', '0');


CREATE TABLE Stats
(
	ServiceID	Integer Primary Key not null,
	Total		integer

);


CREATE TABLE  ServiceTypes
(
	TypeID 			Integer Primary Key not null,
	ParentID		Integer ,
	TypeName		Text COLLATE NOCASE,
	MetadataClass		Text COLLATE NOCASE,
	Description		Text COLLATE NOCASE,
	MainService	  	Integer  default 0

);

insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (0, 'Files', 'File', 'all local files', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (1, 'Folders', 'File', 'folders only', 0 );
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (2, 'Documents', 'Doc, File', 'documents only', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (3, 'Images', 'Image, File', 'image files only', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (4, 'Music', 'Audio, File', 'music files only', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (5, 'Videos', 'File', 'video and movie files only', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (6, 'Text Files', 'File', 'text files only', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (7, 'Development Files', 'File', 'development and source code files only', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (8, 'Other Files', 'File', 'all other uncategorized files', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (9, 'VFS Files', 'File', 'all VFS based files', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (10, 'VFS Folders', 'File', 'VFS based folders only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (11, 'VFS Documents', 'Doc, File', 'VFS based documents only',0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (12, 'VFS Images', 'Image, File', 'VFS based images only',0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (13, 'VFS Music', 'Audio, File', 'VFS based music only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (14, 'VFS Videos', 'File', 'VFS based movies/videos only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (15, 'VFS Text', 'File', ' VFS based text files only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (16, 'VFS Development Files', 'File', 'VFS based development and source code files only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (17, 'VFS Other Files', 'File', 'VFS based folders only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (18, 'Conversations', 'File', 'IM logs and conversations only', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (19, 'Playlists', 'PlayList', 'playlists only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (20, 'Applications', 'App', 'applications only', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (21, 'Contacts', 'Contact', 'contacts only', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (22, 'Emails', 'Email', 'emails only', 1);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (23, 'EmailAttachments', 'File', 'email attachments only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (24, 'Notes', 'Note', 'notes only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (25, 'Appointments', 'Appointment', 'appointments only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (26, 'Tasks', 'Task', 'tasks and to-do lists only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (27, 'Bookmarks', 'Bookmark', 'bookmarks only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (28, 'History', 'History', 'history only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (29, 'Projects', 'Project', 'projects only', 0);
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (30, 'Web Pages', 'Web', 'Visited web pages only', 0);

/* store volume and HAL info here for files */
CREATE TABLE  Volumes
(
	VolumeID 	Integer primary key AUTOINCREMENT not null,
	UDI		Text,
	VolumeName	Text,
	MountPath	Text,
	Enabled		Integer default 0

);


/* basic info for a file or service object */
CREATE TABLE  Services
(
	ID            		Integer primary key AUTOINCREMENT not null,
	ServiceTypeID		Integer  default 0, /* see ServiceTypes table above for ID values */
	SubType			Integer default 0, /* reserved for future use */
	Path 			Text  not null, /* non-file objects should use service name here */
	Name	 		Text , /* name of file or object - the combination path and name must be unique for all objects */
	Mime			Text,
	Size			Integer,
	Enabled			Integer default 1,
	IsDirectory   		Integer default 0,
	IsWatchedDirectory	Integer default 0,
    	IsLink        		Integer default 0,
	IsVfs			Integer default 0,
	AuxilaryID		Integer, /* link to Volumes table for files, link to mbox table for emails*/
	IndexTime  		Integer, /* should equal st_mtime for file if up-to-date */
	Offset			Integer, /* last used disk offset for indexable files that always grow (like chat logs) or email offset */

    	unique (Path, Name)

);

CREATE INDEX  ServiceIndex1 ON Services (ServiceTypeID);
CREATE INDEX  ServiceIndex2 ON Services (AuxilaryID);
CREATE INDEX  ServiceIndex3 ON Services (Mime);
CREATE INDEX  ServiceIndex4 ON Services (Size);


/* provides links from one service entity to another */
CREATE TABLE  ServiceLinks
(
	ServiceID		Integer not null,
	LinkID			Integer not null,
	LinkTypeID		Integer not null, /* see ServiceLinkTypes table */

	primary key (ServiceID, LinkID, LinkTypeID)

);

CREATE TABLE  ServiceLinkTypes
(
	ID			Integer primary key AUTOINCREMENT not null,
	Type			Text  COLLATE NOCASE

);

insert Into ServiceLinkTypes (Type) Values ('PlayListItem');



/* de-normalised metadata which is unprocessed (not normalized or casefolded) for display only - never used for searching */
CREATE TABLE  ServiceMetaDataDisplay 
(
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	MetaDataValue     	Text,
	EmbeddedFlag		Integer default 0,
	DeleteFlag		Integer default 0,

	primary key (ServiceID, MetaDataID)
);



/* utf-8 based metadata that is used for searching */
CREATE TABLE  ServiceMetaData 
(
	ID			Integer primary key AUTOINCREMENT not null,
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	MetaDataValue     	Text COLLATE UTF8, 
	EmbeddedFlag		Integer default 0,
	DeleteFlag		Integer default 0

);

CREATE INDEX  ServiceMetaDataIndex1 ON ServiceMetaData (ServiceID);
CREATE INDEX  ServiceMetaDataIndex2 ON ServiceMetaData (MetaDataID);


/* metadata for all keyword types - keywords are db indexed for fast searching */
CREATE TABLE  ServiceKeywordMetaData 
(
	ID			Integer primary key AUTOINCREMENT not null,
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	MetaDataValue		Text COLLATE UTF8, 
	EmbeddedFlag		Integer default 0,
	DeleteFlag		Integer default 0
	
);

CREATE INDEX  ServiceKeywordMetaDataIndex1 ON ServiceKeywordMetaData (MetaDataID, MetaDataValue);
CREATE INDEX  ServiceKeywordMetaDataIndex2 ON ServiceKeywordMetaData (ServiceID);



/* numerical metadata including DateTimes are stored here */
CREATE TABLE  ServiceNumericMetaData 
(
	ID			Integer primary key AUTOINCREMENT not null,
	ServiceID		Integer not null,
	MetaDataID 		Integer not null,
	MetaDataValue		real,
	EmbeddedFlag		Integer default 0,
	DeleteFlag		Integer default 0
);

CREATE INDEX  ServiceNumericMetaDataIndex1 ON ServiceNumericMetaData (ServiceID);
CREATE INDEX  ServiceNumericMetaDataIndex2 ON ServiceNumericMetaData (MetaDataID, MetaDataValue);



/* blob data is never searched and can contain embedded Nulls */
CREATE TABLE  ServiceBlobMetaData 
(
	ID			Integer primary key AUTOINCREMENT not null,
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	MetaDataValue		blob,
	BlobLength		Integer,
	EmbeddedFlag		Integer default 0,
	DeleteFlag		Integer default 0
);

CREATE INDEX ServiceBlobMetaDataIndex1 ON ServiceBlobMetaData (ServiceID);



/* describes the types of metadata */
CREATE TABLE  MetaDataTypes 
(
	ID	 		Integer primary key AUTOINCREMENT not null,
	MetaName		Text  not null  COLLATE NOCASE, 
	DataTypeID		Integer  not null, /* 0=indexable string, 1= non-indexable string, 2=numeric, 3=datetime, 4=Blob, 5=keyword */
	MultipleValues		Integer default 1, /* 0= type cannot have multiple values per entity, 1= type can have more than 1 value per entity */
	Weight			Integer  default 1 not null,  /* weight of metdata type in ranking */

	Unique (MetaName)
);


/* flattened table to store metadata inter-relationships */
CREATE TABLE  MetaDataChildren
(
	MetaDataID		integer  not null,
	ChildID			integer not null,

	primary key (MetaDataID, ChildID)

);


/* built in metadata types and their relationships*/

begin transaction;

/* Global generic Dublin Core types applicable to all metadata classes */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Title', 5, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Date', 3, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Modified', 3, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Created', 3, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Format', 0, 1, 20);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Creator', 0, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Rights', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:License', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Publisher', 0, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Identifier', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Source', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Relation', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Language', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Keywords', 5, 1, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Coverage', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Description', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Contributors',0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Type',0, 1, 10);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Date' and C.MetaName = 'DC:Created';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Date' and C.MetaName = 'DC:Modified';

/* File Specific metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Name', 0, 0, 20);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Ext', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:NameDelimited', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Path', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Link', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Mime', 5, 0, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Size', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:License', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Copyright', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Origin', 0, 0, 5);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:OriginURI', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Permissions', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Rank', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:IconPath', 1, 0, 0 );
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:SmallThumbnailPath', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:LargeThumbnailPath', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Modified', 3, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Accessed', 3, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Other', 0, 0, 5);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Identifier' and C.MetaName = 'File:Name';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Identifier' and C.MetaName = 'File:Path';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Format' and C.MetaName = 'File:Ext';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Format' and C.MetaName = 'File:Mime';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Source' and C.MetaName = 'File:Origin';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Source' and C.MetaName = 'File:OriginURI';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:License' and C.MetaName = 'File:License';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:License' and C.MetaName = 'File:Copyright';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Modified' and C.MetaName = 'File:Modified';


/* Audio specific metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Title', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Artist', 0, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Album', 0, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:AlbumArtist', 0, 1, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:AlbumTrackCount', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:TrackNo', 2, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:DiscNo', 2, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Performer', 0, 0, 30);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:TrackGain', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:TrackPeakGain', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:AlbumGain', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:AlbumPeakGain', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Duration', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:ReleaseDate', 3, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Comment', 0, 0, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Genre', 5, 1, 75);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Codec', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:CodecVersion', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Samplerate', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Bitrate', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Channels', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:LastPlay', 3, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:PlayCount', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:IsNew', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:MBAlbumID', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:MBArtistID', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:MBAlbumArtistID', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:MBTrackID', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Lyrics', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:CoverAlbumThumbnailPath', 1, 0, 0);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Audio:Title';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Audio:Artist';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Audio:AlbumArtist';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Created' and C.MetaName = 'Audio:ReleaseDate';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Description' and C.MetaName = 'Audio:Comment';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Type' and C.MetaName = 'Audio:Genre';


/* Document specific metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Title', 0, 0, 60);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Subject', 0, 0, 70);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Author', 0, 0, 60);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Keywords', 5, 1, 80);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Comments', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:PageCount', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:WordCount', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Created', 3, 0, 0);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Doc:Title';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Doc:Subject';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Doc:Author';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Keywords' and C.MetaName = 'Doc:Keywords';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Descritpion' and C.MetaName = 'Doc:Comments';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Created' and C.MetaName = 'Doc:Created';

/* Image specific metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Height', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Width', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Title', 0, 0, 60);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Album', 0, 0, 30);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Date', 3,  0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Keywords', 5, 0, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Creator', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Comments', 0, 0, 20);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Description', 0, 0, 15);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Software', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:CameraMake', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:CameraModel', 0, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Orientation', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:ExposureProgram', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:ExposureTime', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:FNumber', 2 , 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Flash', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:FocalLength', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:ISOSpeed', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:MeteringMode', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:WhiteBalance', 1, 0, 0);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Image:Title';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Created' and C.MetaName = 'Image:Date';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Image:Creator';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Description' and C.MetaName = 'Image:Comments';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Description' and C.MetaName = 'Image:Description';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Keywords' and C.MetaName = 'Image:Keywords';

/* video metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Title', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Author', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Comments', 0, 0, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Height', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Width', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:FrameRate', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Codec', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Bitrate', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Duration', 2, 0, 0);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Video:Title';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Video:Author';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Description' and C.MetaName = 'Video:Comments';

/* email metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Body', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Date', 3, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Sender', 0, 0, 35);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:SentTo', 0, 1, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Attachments', 0, 1, 20);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:AttachmentsDelimted', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:CC', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Subject', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Recipient', 0, 1, 1);


insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Created' and C.MetaName = 'Email:Date';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Email:Sender';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Email:Subject';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'Email:Recipient' and C.MetaName = 'Email:SentTo';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'Email:Recipient' and C.MetaName = 'Email:CC';

end transaction;


CREATE TABLE MBoxes
(
	ID		Integer primary key AUTOINCREMENT not null,
	Path		Text not null,
	Type		Integer default 0, /* 0=unknown, 1=evolution, 2=thunderbird, 3=kmail */
	Offset		Integer,
	LastUri		text,
	MessageCount 	Integer,
	MBoxSize	Integer,
	Mtime		Integer,

	unique (Path)
);



CREATE TABLE LiveQueries
(
	ID		Integer primary key AUTOINCREMENT not null,
	Type		int default 0, /* 0 = no liveness */
	Service		Text,
	SearchTerms	Text,
	Keywords	Text,
	Mimes		Text,
	Folder		Text,	
	vSql		Text,
	vCheckSql	Text,
	vOffset		int,
	vLimit		int,
	FileInsertEvent	int,
	FileDeleteEvent	int,
	FileMoveEvent	int,
	FileChangeEvent	int,
	KeywordsEvent	int,
	MetadataEvent	int
);

CREATE TABLE LiveQueryResults
(
	QueryID		integer not null,
	URI		text not null,

	primary key (QueryID, URI)
);


CREATE TABLE SearchResults1
(
	SID		Integer primary key not null,
	Score		Integer
);


CREATE TABLE SearchResults2
(
	SID		Integer primary key not null,
	Score		Integer
);


/* table for files waiting to be processed */
CREATE TABLE  FilePending
(
	ID			Integer primary key AUTOINCREMENT not null,
	FileID 			Integer default -1,
	Action			Integer default 0,
	PendingDate		Integer,
	FileUri			Text  not null,
	MimeType		Text ,
	IsDir			Integer default 0,
	IsNew			Integer default 0,
	RefreshEmbedded		Integer default 0,
	RefreshContents		Integer default 0,	
	ServiceTypeID		Integer default 0
);


/* temp tables */
CREATE TABLE  FileTemp
(
	ID			Integer primary key not null,
	FileID 			Integer default -1,
	Action			Integer default 0,
	FileUri			Text  not null,
	MimeType		Text ,
	IsDir			Integer default 0,
	IsNew			Integer default 0,
	RefreshEmbedded		Integer default 0,
	RefreshContents		Integer default 0,	
	ServiceTypeID		Integer default 0
);

CREATE TABLE  MetadataTemp
(
	ID			Integer primary key not null,
	FileID 			Integer default -1,
	Action			Integer default 0,
	FileUri			Text  not null,
	MimeType		Text ,
	IsDir			Integer default 0,
	IsNew			Integer default 0,
	RefreshEmbedded		Integer default 0,
	RefreshContents		Integer default 0,	
	ServiceTypeID		Integer default 0
);


CREATE TABLE  FileWatches
(
	WatchID 	Integer not null, 
	URI 		Text not null,  

	primary key (WatchID), 
	unique (URI)
);





/* allow aliasing of VFolders with nice names */
CREATE TABLE  VFolders
(
	Path			Text  not null,
	Name			Text  not null,
	Query			text not null,
	RDF			text,
	Type			Integer default 0,
	active			Integer,

	primary key (Path, Name)

);

ANALYZE;


CREATE TABLE Options (	
	OptionKey 	Text COLLATE NOCASE not null,	
	OptionValue	Text COLLATE NOCASE
);

insert Into Options (OptionKey, OptionValue) values ('DBVersion', '11');
insert Into Options (OptionKey, OptionValue) values ('Sequence', '0');
insert Into Options (OptionKey, OptionValue) values ('UpdateCount', '0');


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
insert Into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values (25, 'AppoIntegerments', 'AppoIntegerment', 'appoIntegerments only', 0);
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


/* basic file info for a file or service object */
CREATE TABLE  Services
(
	ID            		Integer primary key AUTOINCREMENT not null,
	ServiceTypeID		Integer  default 0, /* see ServiceTypes table above for ID values */
	Path 			Text  not null, /* non-file objects should use service name here */
	Name	 		Text , /* name of file or object - the combination path and name must be unique for all objects */
	Mime			Text,
	Size			Integer,
	Enabled			Integer default 1,
	IsDirectory   		Integer default 0,
	IsWatchedDirectory	Integer default 0,
    	IsLink        		Integer default 0,
	IsVfs			Integer default 0,
	VolumeID		Integer default -1,	 /* link to Volumes table */
	IndexTime  		Integer, /* should equal st_mtime for file if up-to-date */
	Offset			Integer, /* last used disk offset for indexable files that always grow (like chat logs) */

    	unique (Path, Name)

);

CREATE INDEX  ServiceIndex ON Services (ServiceTypeID);


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



/* store all keywords here. */
CREATE TABLE  ServiceKeywords
(
	ServiceID		Integer not null,
	Keyword     		Text  not null, 
	EmbeddedFlag		Integer default 0,
	DeleteFlag		Integer default 0,

	Primary Key (ServiceID, Keyword)
);


CREATE INDEX  ServiceKeywordsKeyword ON ServiceKeywords (Keyword);

CREATE TABLE  Keywords
(
	Keyword			Text  not null, 
	Description		Text ,
	CustomEmblem		Text ,
	Status			Integer default 0, /* to be defined */
	
	primary key (Keyword)

);


/* store all metadata here. */
CREATE TABLE  ServiceMetaData 
(
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	MetaDataValue     	Text, 
	EmbeddedFlag		Integer default 0,
	DeleteFlag		Integer default 0,
	
	primary key (ServiceID, MetaDataID)
);


CREATE TABLE  ServiceIndexMetaData 
(
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	MetaDataValue		Text,
	EmbeddedFlag		Integer default 0,
	DeleteFlag		Integer default 0,
	
	primary key (ServiceID, MetaDataID)
);

CREATE INDEX  ServiceIndexMetaDataIndex ON ServiceIndexMetaData (MetaDataID, MetaDataValue);


CREATE TABLE  ServiceNumericMetaData 
(
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	MetaDataValue		real,
	EmbeddedFlag		Integer default 0,
	DeleteFlag		Integer default 0,
	
	primary key (ServiceID, MetaDataID)
);


CREATE INDEX  ServiceNumericMetaDataIndex ON ServiceNumericMetaData (MetaDataID, MetaDataValue);

CREATE TABLE  ServiceBlobMetaData 
(
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	MetaDataValue		text,
	EmbeddedFlag		Integer default 0,
	DeleteFlag		Integer default 0,
	
	primary key (ServiceID, MetaDataID)
);




/* describes the types of metadata */
CREATE TABLE  MetaDataTypes 
(
	ID	 		Integer primary key AUTOINCREMENT not null,
	MetaName		Text  not null  COLLATE NOCASE, 
	DataTypeID		Integer  not null, /* 0=full text indexable string (max 255 long), 1=string or Blob, 2=numeric, 3=datetime, 4== fulltext Indexable Blob, (99=read only indexable) */
	Embedded		Integer not null, /* if the metadata is normally embedded in the file */
	Writeable		Integer not null, /* is metadata writable */
	Weight			Integer  default 1 not null,  /* weight of metdata type in ranking */


	Unique (MetaName)
);


/* built in metadata types */

begin transaction;

insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Keywords', 99, 0, 0, 100);

insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Name', 0, 1, 0, 5);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Path', 0, 1, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Link', 1, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Format', 0, 1, 0, 15);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Size', 2, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Origin', 4, 0, 1, 5);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.OriginURI', 1, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Permissions', 1, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Description', 4, 0, 1, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Rank', 2, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Publisher', 0, 0, 1, 20);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.License', 4, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Contributer', 0, 1, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Rights', 4, 1, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Relation', 0, 1, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Source', 0, 1, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Language', 0, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Identifier', 0, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Coverage', 0, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Copyright', 4, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Creator', 0, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Location', 0, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Organization', 0, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.IconPath', 1, 0, 1, 0 );
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.SmallThumbnailPath', 1, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.LargeThumbnailPath', 1, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Modified', 3, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Accessed', 3, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Other', 4, 1, 0, 5);

insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Title', 0, 1, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Artist', 0, 1, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Album', 0, 1, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.AlbumArtist', 0, 1, 1, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.AlbumTrackCount', 2, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.TrackNo', 2, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.DiscNo', 2, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Performer', 0, 1, 1, 70);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.TrackGain', 2, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.TrackPeakGain', 2, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.AlbumGain', 2, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.AlbumPeakGain', 2, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Duration', 2, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.ReleaseDate', 3, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Comment', 4, 1, 1, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Genre', 0, 1, 1, 90);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Codec', 0, 1, 1, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.CodecVersion', 1, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Samplerate', 2, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Bitrate', 2, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Channels', 2, 1, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.LastPlay', 3, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.PlayCount', 2, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.IsNew', 2, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.MBAlbumID', 1, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.MBArtistID', 1, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.MBAlbumArtistID', 1, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.MBTrackID', 1, 0, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Lyrics', 4, 0, 1, 4);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.CoverAlbumThumbnailPath', 1, 0, 1, 0);

insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Title', 0, 1, 0, 90);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Subject', 0, 1, 0, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Author', 0, 1, 0, 90);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Keywords', 4, 1, 0, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Comments', 4, 1, 0, 80);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.PageCount', 2, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.WordCount', 2, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Created', 3, 1, 0, 0);

insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Height', 2, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Width', 2, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Title', 0, 1, 0, 60);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Album', 0, 0, 1, 30);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Date', 3, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Keywords', 4, 1, 0, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Creator', 0, 1, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Comments', 4, 1, 0, 20);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Description', 4, 1, 0, 15);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Software', 0, 1, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.CameraMake', 0, 1, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.CameraModel', 0, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Orientation', 1, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.ExposureProgram', 1, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.ExposureTime', 2, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.FNumber', 2 , 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Flash', 2, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.FocalLength', 2, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.ISOSpeed', 2, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.MeteringMode', 1, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.WhiteBalance', 1, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Copyright', 4, 1, 0, 1);

insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Video.Title', 0, 1, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Video.Author', 0, 1, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Video.Comment', 4, 1, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Video.Height', 2, 1, 0, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Video.Width', 2, 1, 0, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Video.FrameRate', 2, 1, 0, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Video.Codec', 4, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Video.Bitrate', 2, 1, 0, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Video.Duration', 2, 1, 0, 100);

insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Email.Date', 3, 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Email.Sender', 4, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Email.SentTo', 4, 1, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Email.Subject', 0, 1, 0, 30);


end transaction;

/* people service */
CREATE TABLE People
(
	ID		Integer primary key  not null,
	EmailAddress	text not null COLLATE NOCASE,
	Name		text

);

CREATE Unique INDEX  PeopleEmailAddress ON People (EmailAddress);


CREATE TABLE MBoxes
(
	ID		Integer primary key AUTOINCREMENT not null,
	Path		Text not null,
	Type		Integer default 0, /* 0=unknown, 1=evolution, 2=thunderbird, 3=kmail */
	Offset		Integer,
	MessageCount 	Integer,
	MBoxSize	Integer,
	Mtime		Integer
);


/* email service */
CREATE TABLE Emails
(
	ID		Integer primary key not null,
	URI		Text,
	MBoxID		Integer Not Null,
	ReceivedDate	Integer not null,
	Type		Integer, /* 0 = text, 1 = html */
	Offset		Integer Not Null,
	ReplyID		Integer	
);

CREATE INDEX  EmailMessageID ON Emails (URI);

CREATE TABLE EmailRefs
(
	EmailID			int not null,
	ReferenceEmailID	int not null,
	RefType			int default 0 /* 0 = reference, 1= additional reply to */
);


CREATE TABLE EmailAttachments
(
	ID		Integer primary key AUTOINCREMENT not null,
	EmailID		Integer Not Null,
	FileID		Integer Not Null	
);

CREATE INDEX  AttachEmailID ON EmailAttachments (EmailID);



CREATE TABLE EmailPeople
(
	EmailID		Integer Not Null,
	PeopleID	Integer Not Null,
	IsSender	Integer default 0,
	IsTo		Integer default 0,
	IsCC		Integer default 0,
	IsBCC		Integer default 0,

	primary key (EmailID, PeopleID)
);

CREATE INDEX  EmailPeopleID ON EmailPeople (PeopleID, EmailID);


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


 



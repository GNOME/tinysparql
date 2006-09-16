
create  table if not exists Options
(
	OptionKey 		varchar(50) not null,
	OptionValue		varchar(50),
	Primary Key (OptionValue)
);

insert into Options (OptionKey, OptionValue) values 
('DBVersion', '4');


create procedure GetVersion() SELECT OptionValue FROM Options WHERE OptionKey = 'DBVersion';

create  table if not exists ServiceTypes
(
	TypeID 			tinyint unsigned not null,
	TypeName		varchar (32),
	MetadataClass		varchar (32),
	Description		varchar (128),
	MainService	  	bit  default 0,

	Primary Key (TypeID)
);

insert into ServiceTypes (TypeID, TypeName, MetadataClass, Description, MainService) values 
(0, 'Files', 'File', 'all local files', 1),
(1, 'Folders', 'File', 'folders only', 0 ),
(2, 'Documents', 'Doc, File', 'documents only', 1),
(3, 'Images', 'Image, File', 'image files only', 1),
(4, 'Music', 'Audio, File', 'music files only', 1),
(5, 'Videos', 'File', 'video and movie files only', 1),
(6, 'Text Files', 'File', 'text files only', 1),
(7, 'Development Files', 'File', 'development and source code files only', 1),
(8, 'Other Files', 'File', 'all other uncategorized files', 1),
(9, 'VFS Files', 'File', 'all VFS based files', 0),
(10, 'VFS Folders', 'File', 'VFS based folders only', 0),
(11, 'VFS Documents', 'Doc, File', 'VFS based documents only',0),
(12, 'VFS Images', 'Image, File', 'VFS based images only',0),
(13, 'VFS Music', 'Audio, File', 'VFS based music only', 0),
(14, 'VFS Videos', 'File', 'VFS based movies/videos only', 0),
(15, 'VFS Text', 'File', ' VFS based text files only', 0),
(16, 'VFS Development Files', 'File', 'VFS based development and source code files only', 0),
(17, 'VFS Other Files', 'File', 'VFS based folders only', 0),
(18, 'Conversations', 'File', 'IM logs and conversations only', 1),
(19, 'Playlists', 'PlayList', 'playlists only', 0),
(20, 'Applications', 'App', 'applications only', 1),
(21, 'Contacts', 'Contact', 'contacts only', 1),
(22, 'Emails', 'Email', 'emails only', 1),
(23, 'EmailAttachments', 'File', 'email attachments only', 0),
(24, 'Notes', 'Note', 'notes only', 0),
(25, 'Appointments', 'Appointment', 'appointments only', 0),
(26, 'Tasks', 'Task', 'tasks and to-do lists only', 0),
(27, 'Bookmarks', 'Bookmark', 'bookmarks only', 0),
(28, 'History', 'History', 'history only', 0),
(29, 'Projects', 'Project', 'projects only', 0);


CREATE TABLE sequence (id int unsigned NOT NULL);
INSERT INTO sequence VALUES (1);

/* store volume and HAL info here for files */
create table if not exists Volumes
(
	VolumeID 	int auto_increment not null,
	UDI		varchar (255),
	VolumeName	varchar (255),
	MountPath	varchar (255),
	Enabled		bool default 0,

	primary key (VolumeID)
);


/* basic file info for a file or service object */
create  table if not exists Services
(
	ID            		int unsigned not null,
	ServiceTypeID		tinyint unsigned default 0, /* see ServiceTypes table above for ID values */
	Path 			varchar (200) character set utf8 not null, /* non-file objects should use service name here */
	Name	 		varchar (128) character set utf8, /* name of file or object - the combination path and name must be unique for all objects */
	Enabled			bool default 1,
	IsServiceSource		bool default 0,
	IsDirectory   		bool default 0,
	IsWatchedDirectory	bool default 0,
    	IsLink        		bool default 0,
	IsVfs			bool default 0,
	VolumeID		int default -1,	 /* link to Volumes table */
	IndexTime  		int unsigned, /* should equal st_mtime for file if up-to-date */
	Offset			int unsigned, /* last used disk offset for indexable files that always grow (like chat logs) */

    	primary key (ID),
    	unique key (Path, Name),
	key (ServiceTypeID),
	key (VolumeID)

);




/* provides links from one service entity to another */
create table if not exists ServiceLinks
(
	ServiceID		int unsigned not null,
	LinkID			int unsigned not null,
	LinkTypeID		tinyint unsigned not null, /* see ServiceLinkTypes table */

	primary key (ServiceID, LinkID, LinkTypeID)

);

create table if not exists ServiceLinkTypes
(
	ID			tinyint unsigned auto_increment not null,
	Type			varchar(32),

	primary key (ID)

);

insert into ServiceLinkTypes (Type) Values ('PlayListItem');



/* store all keywords here. */
create  table if not exists ServiceKeywords
(
	ServiceID		int unsigned not null,
	Keyword     		varchar (255) character set utf8 not null, 

	Primary Key (ServiceID, Keyword),
	Key (Keyword)
);



create table if not exists Keywords
(
	Keyword			varchar (255) character set utf8 not null, 
	Description		varchar (255),
	CustomEmblem		varchar (255),
	IsFavouriteTag		bool default 0,
	
	primary key (Keyword)

);


/* store all metadata here. */
create  table if not exists ServiceMetaData 
(
	IndexID			int unsigned auto_increment not null,
	ServiceID		int unsigned not null,
	MetaDataID 		smallint unsigned not null,
	MetaDataValue     	Text character set utf8, 
	MetaDataIndexValue	MediumText character set utf8,
	MetaDataNumericValue	double, 	

	Primary Key (IndexID),
	Key (ServiceID, MetaDataID),
	Key (MetaDataIndexValue (32)),
	key INumericValue (MetaDataID, MetaDataNumericValue),
	FullText INDEX (MetaDataIndexValue)

);




/* store all indexable metadata here. */
create  table if not exists ServiceIndexMetaData 
(
	ServiceID		int unsigned not null,
	MetaDataID 		smallint unsigned not null,
	IndexerID		int unsigned,
	MetaDataIndexValue	varchar (255),
	MetaDataIndexBlob	MediumText,

	Primary Key (ServiceID, MetaDataID),
	Key (MetaDataID, MetaDataIndexValue (32))

);





/* describes the types of metadata */
create  table if not exists MetaDataTypes 
(
	ID	 		smallint unsigned auto_increment not null,
	MetaName		varchar (128) not null, 
	DataTypeID		tinyint unsigned not null, /* 0=full text indexable string (max 255 long), 1=string or Blob, 2=numeric, 3=datetime, 4==IndexBlob (99=special case)*/
	Embedded		bool not null, /* if the metadata is embedded in the file */
	Writeable		bool not null, /* is metadata writable */
	Weight			tinyint unsigned default 1 not null,  /* weight of metdata type in ranking */

	Primary Key (ID),
	Unique (MetaName)
);


/* built in metadata types */

insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Keywords', 99, 0, 0, 100);

insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Content', 99, 1, 0, 1);

insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Name', 0, 1, 0, 5);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Path', 0, 1, 0, 1);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Ext', 0, 1, 0, 50);

insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Link', 1, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Format', 0, 1, 0, 15);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Size', 2, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Origin', 0, 0, 1, 5);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.OriginURI', 1, 0, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Permissions', 1, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Description', 0, 0, 1, 25);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Rank', 2, 0, 1, 0);

insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Publisher', 0, 0, 1, 20);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.License', 0, 1, 0, 10);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Contributer', 0, 0, 1, 10);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Rights', 0, 1, 0, 10);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Relation', 0, 0, 1, 10);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Source', 0, 0, 1, 10);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Language', 0, 1, 0, 10);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Identifier', 0, 0, 1, 10);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Coverage', 0, 0, 1, 10);

insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.IconPath', 1, 0, 1, 0 );
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.SmallThumbnailPath', 1, 0, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.LargeThumbnailPath', 1, 0, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Modified', 3, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Accessed', 3, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('File.Other', 0, 1, 0, 5);

insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Title', 0, 1, 1, 50);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Artist', 0, 1, 1, 50);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Album', 0, 1, 1, 50);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.AlbumArtist', 0, 1, 1, 25);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.AlbumTrackCount', 2, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.TrackNo', 2, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.DiscNo', 2, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Performer', 0, 1, 1, 70);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.TrackGain', 2, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.TrackPeakGain', 2, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.AlbumGain', 2, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.AlbumPeakGain', 2, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Duration', 2, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.ReleaseDate', 3, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Comment', 0, 1, 1, 25);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Genre', 0, 1, 1, 90);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Codec', 0, 1, 1, 1);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.CodecVersion', 1, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Samplerate', 2, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Bitrate', 2, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Channels', 2, 1, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.LastPlay', 3, 0, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.PlayCount', 2, 0, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.IsNew', 2, 0, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.MBAlbumID', 1, 0, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.MBArtistID', 1, 0, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.MBAlbumArtistID', 1, 0, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.MBTrackID', 1, 0, 1, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.Lyrics', 0, 0, 1, 4);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Audio.CoverAlbumThumbnailPath', 1, 0, 1, 0);

insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Title', 0, 1, 0, 90);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Subject', 0, 1, 0, 100);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Author', 0, 1, 0, 90);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Keywords', 0, 1, 0, 100);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Comments', 0, 1, 0, 80);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.PageCount', 2, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.WordCount', 2, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Doc.Created', 3, 1, 0, 0);

insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Height', 2, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Width', 2, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Title', 0, 1, 0, 60);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Album', 0, 0, 1, 30);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Date', 3, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Keywords', 0, 1, 0, 100);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Creator', 0, 1, 0, 50);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Comments', 0, 1, 0, 20);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Description', 0, 1, 0, 15);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Software', 0, 1, 0, 1);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.CameraMake', 0, 1, 0, 1);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.CameraModel', 0, 1, 0, 10);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Orientation', 1, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.ExposureProgram', 1, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.ExposureTime', 2, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.FNumber', 2 , 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Flash', 2, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.FocalLength', 2, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.ISOSpeed', 2, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.MeteringMode', 1, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.WhiteBalance', 1, 1, 0, 0);
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable, Weight) values  ('Image.Copyright', 0, 1, 0, 1);



/* optional contextual file data - gives a nice audit trail for a file */
create  table if not exists FileContexts 
(
	FileID			int unsigned not null,
	ContextActionID		tinyint unsigned not null, 
	ContextDate  		DateTime not null,
	ContextApp		varchar (128),
	ContextFileID		int unsigned, 		  /* file if linked/attached/embedded */

	primary key (FileID, ContextActionID, ContextDate),
	key (ContextDate),
	key (ContextFileID)
);

create  table if not exists FileContextActions
(
	ID			tinyint unsigned auto_increment not null, 
	ContextAction  		varchar (30) not null,

	primary key (ID)
);

insert into FileContextActions (ContextAction) values ('Created'),('Edited'),('Attached'),('Embedded'),('Downloaded');



/* table for files waiting to be processed */
create table if not exists FilePending
(
	ID			int auto_increment not null,
	FileID 			int default -1,
	Action			tinyint default 0,
	PendingDate		BIGINT,
	FileUri			varchar (255) not null,
	MimeType		varchar (64),
	IsDir			tinyint default 0,

	primary key (ID),
	key (FileID, Action),
	key (PendingDate),
	key (FileUri)
);


create table if not exists FileWatches
(
	WatchID int not null, 
	URI varchar(255) not null,  

	primary key (WatchID), 
	unique (URI)
);



/* allow aliasing of VFolders with nice names */
create table if not exists VFolders
(
	Path			varchar (200) not null,
	Name			varchar (128) not null,
	Query			text not null,
	RDF			text,

	primary key (Path, Name)

);





select Null

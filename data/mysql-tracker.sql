
create  table if not exists Options
(
	OptionKey 		varchar(50) not null,
	OptionValue		varchar(50),
	Primary Key (OptionValue)
);

insert into Options (OptionKey, OptionValue) values 
('DBVersion', '1');


create procedure GetVersion() SELECT OptionValue FROM Options WHERE OptionKey = 'DBVersion';

create  table if not exists ServiceTypes
(
	TypeID 		tinyint unsigned not null,
	TypeName	varchar (16),
	MetadataClass	varchar (16),
	TableName	varchar (16),

	Primary Key (TypeID)
);

insert into ServiceTypes (TypeID, TypeName, MetadataClass) values 
(0, 'Files', 'File'),
(1, 'Documents', 'Doc'),
(2, 'Images', 'Image'),
(3, 'Music', 'Audio'),
(4, 'Videos', 'File'),
(5, 'VFSFiles', 'File'),
(6, 'VFSDocuments', 'Doc'),
(7, 'VFSImages', 'Image'),
(8, 'VFSMusic', 'Audio'),
(9, 'VFSVideos', 'File'),
(10, 'Conversations', 'File'),
(11, 'Playlists', 'PlayList'),
(12, 'Applications', 'App'),
(13, 'Contacts', 'Contact'),
(14, 'Emails', 'Email'),
(15, 'EmailAttachments', 'File'),
(16, 'Notes', 'Note'),
(17, 'Appointments', 'Appointment'),
(18, 'Tasks', 'Task'),
(19, 'Bookmarks', 'Bookmark'),
(20, 'History', 'History'),
(21, 'Projects', 'Project');


CREATE TABLE sequence (id int unsigned NOT NULL);
INSERT INTO sequence VALUES (0);


/* basic file info for a file or service object */
create  table if not exists Services
(
	ID            		int unsigned not null,
	ServiceTypeID		tinyint unsigned default 0, /* see ServiceTypes table above for ID values */
	Path 			varchar (200) character set utf8 not null, /* non-file objects should use service name here */
	Name	 		varchar (128) character set utf8, /* name of file or object - the combination path and name must be unique for all objects */
	IsServiceSource		bool default 0,
	IsDirectory   		bool default 0,
	IsWatchedDirectory	bool default 0,
    	IsLink        		bool default 0,
	Misc			varchar(255), 
	MiscInt			int,
	MiscDate		DateTime,
	IndexTime  		int unsigned, /* should equal st_mtime for file if up-to-date */
	Offset			int unsigned, /* last used disk offset for indexable files that always grow (like chat logs) */

    	primary key (ID),
    	unique key (Path, Name),
	key (ServiceTypeID)

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
	Keyword     		varchar (32) character set utf8 not null, 

	Primary Key (ServiceID, Keyword),
	Key (Keyword)
);




/* store all metadata here. */
create  table if not exists ServiceMetaData 
(
	ServiceID		int unsigned not null,
	MetaDataID 		smallint unsigned not null,
	MetaDataValue     	Text character set utf8, 
	MetaDataIndexValue	MediumText character set utf8,
	MetaDataNumericValue	double, 	

	Primary Key (ServiceID, MetaDataID),
	Key (MetaDataIndexValue (24)),
	key INumericValue (MetaDataID, MetaDataNumericValue),
	FullText (MetaDataIndexValue)

);




/* describes the types of metadata */
create  table if not exists MetaDataTypes 
(
	ID	 		smallint unsigned auto_increment not null,
	MetaName		varchar (128) not null, 
	DataTypeID		tinyint unsigned, /* 0=full text indexable string, 1=string, 2=numeric, 3=datetime (as string) */
	Embedded		bool, /* if the metadata is embedded in the file */
	Writeable		bool, /* is metadata writable */

	Primary Key (ID),
	Unique (MetaName)
);


/* built in metadata types */
insert into MetaDataTypes (MetaName, DatatypeID, Embedded, Writeable) values 
('File.Name', 0, 1, 0),
('File.Path', 0, 1, 0),
('File.Link', 1, 1, 0),
('File.Format', 0, 1, 0 ),
('File.Size', 2, 1, 0),
('File.Permissions', 1, 1, 0),
('File.Content', 0, 1, 0),
('File.Description', 0, 0, 1),
('File.Keywords', 0, 0, 1),
('File.Rank', 2, 0, 1),
('File.IconPath', 1, 0, 1 ),
('File.SmallThumbnailPath', 1, 0, 1),
('File.LargeThumbnailPath', 1, 0, 1),
('File.Modified', 3, 1, 0),
('File.Accessed', 3, 1, 0 ),
('File.Other', 0, 1, 0 ),
('Audio.Title', 0, 1, 1),
('Audio.Artist', 0, 1, 1),
('Audio.Album', 0, 1, 1),
('Audio.AlbumArtist', 0, 1, 1),
('Audio.AlbumTrackCount', 2, 1, 1),
('Audio.TrackNo', 2, 1, 1),
('Audio.DiscNo', 2, 1, 1),
('Audio.Performer', 0, 1, 1),
('Audio.TrackGain', 2, 1, 1),
('Audio.TrackPeakGain', 2, 1, 1),
('Audio.AlbumGain', 2, 1, 1),
('Audio.AlbumPeakGain', 2, 1, 1),
('Audio.Duration', 2, 1, 0),
('Audio.ReleaseDate', 3, 1, 1),
('Audio.Comment', 0, 1, 1),
('Audio.Genre', 0, 1, 1),
('Audio.Codec', 0, 1, 1),
('Audio.CodecVersion', 1, 1, 1),
('Audio.Samplerate', 2, 1, 1),
('Audio.Bitrate', 2, 1, 1),
('Audio.Channels', 2, 1, 1),
('Audio.LastPlay', 3, 0, 1),
('Audio.PlayCount', 2, 0, 1),
('Audio.IsNew', 2, 0, 1),
('Audio.MBAlbumID', 1, 0, 1),
('Audio.MBArtistID', 1, 0, 1),
('Audio.MBAlbumArtistID', 1, 0, 1),
('Audio.MBTrackID', 1, 0, 1),
('Audio.Lyrics', 0, 0, 1),
('Audio.CoverAlbumThumbnailPath', 1, 0, 1),
('Doc.Title', 0, 1, 0),
('Doc.Subject', 0, 1, 0),
('Doc.Author', 0, 1, 0),
('Doc.Keywords', 0, 1, 0),
('Doc.Comments', 0, 1, 0),
('Doc.PageCount', 2, 1, 0),
('Doc.WordCount', 2, 1, 0),
('Doc.Created', 3, 1, 0),
('Image.Height', 2, 1, 0),
('Image.Width', 2, 1, 0),
('Image.Title', 0, 1, 0),
('Image.Date', 3, 1, 0),
('Image.Keywords', 0, 1, 0),
('Image.Creator', 0, 1, 0),
('Image.Comments', 0, 1, 0),
('Image.Description', 0, 1, 0),
('Image.Software', 0, 1, 0),
('Image.CameraMake', 0, 1, 0),
('Image.CameraModel', 0, 1, 0),
('PlayList.DateCreated', 3, 0, 1),
('PlayList.LastPlay', 3, 0, 1),
('PlayList.PlayCount', 2, 0, 1),
('PlayList.Description', 0, 0, 1);


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
	Counter			tinyint default 0,
	FileUri			varchar (255) not null,
	MimeType		varchar (64),
	IsDir			tinyint default 0,

	primary key (ID),
	key (FileID, Action),
	key (Counter)
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

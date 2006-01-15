/* WatchType Values : */
/* 0 = top level watched folder */
/* 1 = watched subfolder */
/* 2 = watched special folder (like say /usr/share/applications for .Desktop files) */
/* 3 = watched special file */
/* 4 = no index folder */
/* 5 = other */

/* basic file info for a file */
create  table if not exists Files
(
	ID            		int unsigned auto_increment not null,
	Path 			varchar (200) character set utf8 not null, /* non-local files can prefix uri type here */
	FileName	 	varchar (128) character set utf8 not null,
	FileTypeID		tinyint unsigned default 0,
	IsVFS			bool default 0,
	IsDirectory   		bool default 0,
    	IsLink        		bool default 0, 
	IsWatched		bool default 0,
	WatchType		tinyint unsigned default 5,
    	IndexTime  		int unsigned, /* should equal st_mtime for file if up-to-date */
	Offset			int unsigned, /* last used disk offset for indexable files that always grow (like chat logs) */

    	primary key (ID),
    	unique key (Path, FileName),
	key (WatchType)

);


/* store all metadata here. */
create  table if not exists FileMetaData 
(
	FileID 			int unsigned not null,
	MetaDataID 		smallint unsigned not null,
	MetaDataValue     	Text character set utf8, 
	MetaDataIndexValue	MediumText character set utf8,
	MetaDataIntegerValue	int unsigned,	

	Primary Key (FileID, MetaDataID),
	key IValue (MetaDataID, MetaDataValue (20)),
	key IIntValue (MetaDataID, MetaDataIntegerValue),
	FullText (MetaDataIndexValue)

);

/* describes the types of metadata */
create  table if not exists MetaDataTypes 
(
	ID	 		smallint unsigned auto_increment not null,
	MetaName		varchar (128) not null, 
	DataTypeID		tinyint unsigned, /* 0=string, 1=int, 2=datetime (as string) */
	Indexable		bool, /* if the metadata uses a full text index*/
	Writeable		bool, /* embedded metadata is not writable */

	Primary Key (ID),
	Unique (MetaName)
);

/* built in metadata types */
insert into MetaDataTypes (MetaName, DatatypeID, Indexable, Writeable) values 
('File.Name', 0, 1, 0),
('File.Path', 0, 1, 0),
('File.Link', 0, 1, 0),
('File.Format', 0, 1, 0 ),
('File.Size', 1, 0, 0),
('File.Permissions', 0, 0, 0),
('File.Content', 0, 1, 0),
('File.Description', 0, 1, 1),
('File.Keywords', 0, 1, 1),
('File.Rank', 1, 0, 1 ),
('File.IconPath', 0, 0, 1 ),
('File.PreviewThumbnailPath', 0, 0, 1),
('File.Modified', 2, 0, 0),
('File.Accessed', 2, 0, 0 ),
('File.Other', 0, 1, 0 ),
('Audio.Title', 0, 1, 0),
('Audio.Artist', 0, 1, 0),
('Audio.Album', 0, 1, 0),
('Audio.Year', 1, 0, 0),
('Audio.Comment', 0, 1, 0),
('Audio.Genre', 0, 1, 0),
('Audio.Codec', 0, 1, 0),
('Audio.Samplerate', 1, 0, 0),
('Audio.Bitrate', 1, 0, 0),
('Audio.Channels', 1, 0, 0),
('Doc.Title', 0, 1, 0),
('Doc.Subject', 0, 1, 0),
('Doc.Author', 0, 1, 0),
('Doc.Keywords', 0, 1, 0),
('Doc.Comments', 0, 1, 0),
('Doc.PageCount', 1, 0, 0),
('Doc.WordCount', 1, 0, 0),
('Doc.Created', 2, 0, 0),
('Image.Height', 1, 0, 0),
('Image.Width', 1, 0, 0),
('Image.Title', 0, 1, 0),
('Image.Date', 2, 0, 0),
('Image.Keywords', 0, 1, 0),
('Image.Creator', 0, 1, 0),
('Image.Comments', 0, 1, 0),
('Image.Description', 0, 1, 0),
('Image.Software', 0, 1, 0),
('Image.CameraMake', 0, 1, 0),
('Image.CameraModel', 0, 1, 0);



/* optional contextual file data - gives a nice audit trail for a file */
create  table if not exists FileContexts 
(
	FileID 			int unsigned not null,
	ContextActionID		tinyint unsigned not null, /* 0=created, 1=edited, 2=attached, 3=embedded */
	ContextDate  		DateTime not null,
	ContextApp		varchar (128),
	ContextFileID		int unsigned, 		  /* file if linked/attached/embedded */

	primary key (FileID, ContextActionID, ContextDate),
	key (ContextDate),
	key (ContextFileID)
);


/* allow aliasing of VFolders with nice names */
create table if not exists VFolders
(
	Path			varchar (200) not null,
	Name			varchar (128) not null,
	Query			text not null,
	RDF			text,
	UserDefined		bool,

	primary key (Path, Name)

);

/* determines whether a file is more than just a file */
create table if not exists FileTypes
(
	ID			tinyint unsigned not null,
	TypeName		varchar(20) not null,

	primary key (ID),
	unique (TypeName)
);

insert into FileTypes values 
(0,'File'),
(1,'Desktop'),
(2,'Bookmarks'),
(3,'SmartBookmarks'),
(4,'WebHistory'),
(5,'Emails'),
(6,'Conversations'),
(7,'Contacts');


/* table for files waiting to be processed */
create table if not exists FilePending
(
	ID			int auto_increment not null,
	FileID 			int default -1,
	Action			tinyint default 0,
	Counter			tinyint default 0,
	FileUri			varchar (255) not null,
	MimeType		varchar (64),
	IsDir			bool default 0,

	primary key (ID),
	key (FileID),
	key (Counter)
);

/* freedesktop .desktop files */
create table if not exists DesktopFiles
(
	AppName			varchar (128) not null,
	LocaleName		varchar (128),
	Comment			varchar (255),
	LocaleComment		varchar (255),
	Categories		varchar (255),
	Executable		varchar (255),

	primary key (AppName),
	FullText (AppName, LocaleName, Comment, LocaleComment, Categories)

);

/* any type of bookmark (web/filesystem) */
create table if not exists Bookmarks
(
	ID			int auto_increment not null,
	Type			smallint not null, /*0=web, 1=Filesystem */
	Title			varchar (255) not null, 
	URL			varchar (255) not null, 

	primary key (ID),
	FullText (Title, URL)
	
);

/* web history */
create table if not exists History
(
	ID			int auto_increment not null,
	HistoryDate		datetime,
	Title			varchar (255) not null, 
	URL			varchar (255) not null, 

	primary key (ID),
	key (HistoryDate),
	FullText (Title, URL)	
);


/* index emails */
create table if not exists Emails
(
	EmailID			int auto_increment not null,
	MessageID		varchar (255) not null, 
	InReplyTo		varchar (255),
	Subject			varchar (255), 
	Body			LongText, 

	primary key (EmailID),
	key (MessageID),
	key (InReplyTo),
	FullText (Subject, Body)
	
);

create table if not exists EmailAttachments
(
	EmailID			int not null,
	AttachmentID		int not null,
	MimeVersion		varchar (30),
	ContentType		varchar (255),
	ContentBoundary		varchar (255),
	ContentCharset		varchar (128),
	ContentEncoding 	varchar (255),
	ContentName		varchar (255),
	ContentDisposition	varchar (255),
	ContentFileName		varchar (255),
	Contents		LongText,
	AsText			LongText,

	primary key (EmailID, AttachmentID),
	FullText (ContentFileName, AsText)
);


create table if not exists EmailAddresses
(
	EmailAddressID		int auto_increment not null,
	EmailName		varchar (255),
	Address			varchar (255) not null,

	primary key (EmailAddressID),
	FullText (EmailName, Address)
);

/* used for any field that can contain multiple emails */
create table if not exists EmailReferences
(
	EmailID			int not null,
	EmailAddressID		int not null,
	ReferenceType		smallint not null, /* 0 = Sent to, 1=CC, 2=BCC, 3=Reference, 4=sender */
	counter			int not null,

	primary key (EmailID, EmailAddressID, ReferenceType, counter),
	key (EmailAddressID)
);

/* index chat logs line by line */
create table if not exists Conversations
(
	FileID			int not null,
	LineID			int not null,
	LogDate			datetime not null, 
	SpeakerNick		varchar (64)  not null,
	Content			varchar (255) not null,
	
	primary key (FileID, LineID),
	FullText (SpeakerNick, Content)
);

create table if not exists Contacts
(
	ContactID		int auto_increment not null,
	ContactName		varchar (255) not null,
	Nick			varchar (30),
	HomeTel			varchar(30),
	BusTel			varchar(30),
	Mobile			varchar(30),
	Fax			varchar(30),
	HomePage		varchar(255),
	ChatAddress		varchar (64),
	ChatAddress2		varchar (64),

	primary key (ContactID),
	FullText (ContactName, Nick)

);

/* Multiple email accounts for a contact */
create table if not exists ContactEmails
(
	ContactID		int not null,
	EmailID			int not null,
	AdressType		smallint not null, /* 0=unknown, 1=home/personal, 2=work */	

	primary key (ContactID, EmailID, AdressType ),
	key (EmailID)
	

);

/* Multiple IM accounts for a contact */
create table if not exists ContactChats
(
	ContactID		int not null,
	ServerID		smallint not null, /* 0 = aim, 1=jabber, 2=yahoo, 3=MSN, 4=ICQ, 5=Groupwise */
	Account			varchar (255) not null,	

	primary key (ContactID, ServerID)

);



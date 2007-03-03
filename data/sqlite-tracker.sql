
CREATE TABLE Options (	
	OptionKey 	Text COLLATE NOCASE not null,	
	OptionValue	Text COLLATE NOCASE
);

insert Into Options (OptionKey, OptionValue) values ('DBVersion', '16');
insert Into Options (OptionKey, OptionValue) values ('Sequence', '1');
insert Into Options (OptionKey, OptionValue) values ('UpdateCount', '0');


/* store volume and HAL info here for files */
CREATE TABLE  Volumes
(
	VolumeID 	Integer primary key AUTOINCREMENT not null,
	UDI		Text,
	VolumeName	Text,
	MountPath	Text,
	Enabled		Integer default 0

);


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



CREATE TABLE KeywordImages
(
	Keyword 	Text primary key,
	Image		Text
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

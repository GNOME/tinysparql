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
	FileID 			Integer default 0,
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
	FileID 			Integer default 0,
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
	FileID 			Integer default 0,
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


CREATE TABLE Events
(
	ID		Integer primary key autoincrement,
	ServiceID	Integer not null,
	BeingHandled	Integer default 0,
	EventType	Text
);

CREATE TABLE LiveSearches
(
	ServiceID	Integer not null,
	SearchID	Text,

	Unique (ServiceID, SearchID)
);

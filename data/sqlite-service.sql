/* basic info for a file or service object */
CREATE TABLE  Services
(
	ID            		Integer primary key AUTOINCREMENT not null,
	ServiceTypeID		Integer  default 0, /* see ServiceTypes table above for ID values */
	SubType			Integer default 0, /* reserved for future use */
	Path 			Text  not null, /* non-file objects should use service name here */
	Name	 		Text, /* name of file or object - the combination path and name must be unique for all objects */
	Mime			Text,
	Size			Integer,
	CanRead			Integer default 1,
	CanWrite		Integer default 1,
	CanExecute		Integer default 1,
	CustomIconFile		Text,
	LanguageId		Integer default 0,
	Enabled			Integer default 1,
	IsDirectory   		Integer default 0,
    	IsLink        		Integer default 0,
	AuxilaryID		Integer, /* link to Volumes table for files, link to mbox table for emails*/
	IndexTime  		Integer, /* should equal st_mtime for file if up-to-date */
	Offset			Integer, /* last used disk offset for indexable files that always grow (like chat logs) or email offset */
	UID			Integer,
	Hash			text,
	ChildFile		text,

    	unique (Path, Name)

);

CREATE INDEX  ServiceIndex1 ON Services (ServiceTypeID);
CREATE INDEX  ServiceIndex2 ON Services (AuxilaryID);

CREATE TABLE MailSummary
(
	ID		Integer primary key AUTOINCREMENT not null,
	MailApp		Integer not null,
	MailType	Integer not null,
	FileName	Text not null,
	Path		Text not null,
	UriPrefix	Text,
	NeedsChecking	Integer default 0,
	MailCount	Integer,
	JunkCount	Integer,
	DeleteCount	Integer,
	Offset		Integer,
	LastOffset	Integer,
	MTime		integer,

	unique (Path)
);


CREATE TABLE JunkMail
(
	UID			integer not null,
	SummaryID		Integer not null,

	primary key (UID, SummaryID)
);



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
	MetaDataValue     	Text,
	MetaDataDisplay		Text, 
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
	MetaDataValue		Text, 
	MetaDataDisplay		Text, 
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

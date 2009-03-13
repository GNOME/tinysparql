
CREATE TABLE  ServiceTypes
(
	TypeID 			Integer primary key AUTOINCREMENT not null,
	TypeName		Text COLLATE NOCASE not null,

	TypeCount		Integer default 0,

	DisplayName		Text default ' ',
	Parent			Text default ' ',
	PropertyPrefix		Text default ' ',
	Enabled			Integer default 1, 
	Embedded		Integer default 1, /* service is created by the indexer if embedded. User or app defined services are not embedded */
	ChildResource		Integer default 0, /* service is a child service */
	
	CreateDesktopFile	Integer default 0, /* used by a UI to indicate whether it should create a desktop file for the service if its copied (using the ViewerExec field + uri) */

	/* useful for a UI when determining what actions a hit can have */
	CanCopy			Integer default 1, 
	CanDelete		Integer default 1,

	ShowServiceFiles	Integer default 0,
	ShowServiceDirectories  Integer default 0,

	HasMetadata		Integer default 1,
	HasFullText		Integer default 1,
	HasThumbs		Integer default 1,
	
	ContentMetadata		Text default ' ', /* the content field is the one most likely to be used for showing a search snippet */ 

	KeyMetadata1		Text default ' ', /* the most commonly requested metadata (especially for tables/grid views) is cached int he services table for extra fast retrieval */
	KeyMetadata2		Text default ' ',
	KeyMetadata3		Text default ' ',
	KeyMetadata4		Text default ' ',
	KeyMetadata5		Text default ' ',
	KeyMetadata6		Text default ' ',
	KeyMetadata7		Text default ' ',
	KeyMetadata8		Text default ' ',
	KeyMetadata9		Text default ' ',
	KeyMetadata10		Text default ' ',
	KeyMetadata11		Text default ' ',

	UIVisible		Integer default 0,	/* should service appear in a search GUI? */
	UITitle			Text default ' ',	/* title format as displayed in the metadata tile */
	UIMetadata1		Text default ' ',	/*UI fields to show in GUI for a hit - if not set then Name,Path,Mime are used */
	UIMetadata2		Text default ' ',
	UIMetadata3		Text default ' ',
	UIView			Text default 'default',

	Description		Text default ' ',
	Database		integer default 0, /* 0 = DB_FILES, 1 = DB_EMAILS, 2 = DB_MISC, 3 = DB_USER */
	Icon			Text default ' ',

	IndexerExec		Text default ' ',
	IndexerOutput		Text default 'stdout',
	ThumbExec		Text default ' ',
	ViewerExec		Text default ' ',

	WatchFolders		Text default ' ',
	IncludeGlob		Text default ' ',
	ExcludeGlob		Text default ' ',

	FileName		Text default ' ',

	unique (TypeName)
);

insert into ServiceTypes (TypeName) values ('default');

/* metadata that should appear in a tabular view and/or metadata tile for the service */
CREATE TABLE ServiceTileMetadata
(
	ServiceTypeID		Integer not null,
	MetaName		Text not null,

	primary key (ServiceTypeID, MetaName)
);


CREATE TABLE ServiceTabularMetadata
(
	ServiceTypeID		Integer not null,
	MetaName		Text not null,

	primary key (ServiceTypeID, MetaName)
);


/* option sspecific to a certain service type go here */
CREATE TABLE ServiceTypeOptions
(
	ServiceTypeID		Integer not null,
	OptionName		Text not null,
	OptionValue		Text default ' ',

	primary key (ServiceTypeID, OptionName)
);




/* these two only apply to file based services */
CREATE TABLE  FileMimes
(
	Mime			Text primary key not null,
	ServiceTypeID		Integer default 0,
	ThumbExec		Text default ' ',
	MetadataExec		Text default ' ',
	FullTextExec		Text default ' '

);

CREATE TABLE  FileMimePrefixes
(
	MimePrefix		Text primary key not null,
	ServiceTypeID		Integer default 0,
	ThumbExec		Text default ' ',
	MetadataExec		Text default ' ',
	FullTextExec		Text default ' '

);



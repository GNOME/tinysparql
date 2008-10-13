
CREATE TABLE  XesamMetaDataTypes 
(
	ID	 		Integer primary key AUTOINCREMENT not null,
	MetaName		Text not null  COLLATE NOCASE, 
	DataTypeID		Integer default 0,
	DisplayName		text,
	Description		text default ' ',
	Enabled			integer default 1, /* used to prevent use of this metadata type */
	UIVisible		integer default 0, /* should this metadata type be visible in a search criteria UI  */
	WriteExec		text default ' ', /* used to specify an external program that can write an *embedded* metadata to a file */
	Alias			text default ' ', /* alternate name for this type (XESAM specs?) */
	FieldName		text default ' ', /* filedname if present in the services table */
	Weight			Integer default 1, /* weight of metdata type in ranking */
	Embedded		Integer default 1, /* 1 if metadata extracted from the file by the indexer and is not updateable by the user. 0 - this metadata can be updated by the user and is external to the file */
	MultipleValues		Integer default 0, /* 0= type cannot have multiple values per entity, 1= type can have more than 1 value per entity */
	Delimited		Integer default 0, /* if 1, extra delimiters (hyphen and underscore) are used to break word */
	Filtered		Integer default 1, /* if 1, words are filtered for numerics (if numeric indexing is disabled), stopwords and min length */
	Abstract		Integer default 0, /* if 0, can be used for storing metadata - Abstract type classes cannot store metadata and can only be used for searching its decendants */
	StemMetadata		Integer default 1, /* 1 if metadata should be stemmed */
	SideCar			Integer default 0, /* should this metadata be backed up in an xmp sidecar file */
	FileName		Text default ' ',

	Categories		text default ' ',
	Parents			text default ' ',

	Unique (MetaName)
);

CREATE TABLE  XesamServiceTypes
(
	TypeID 			Integer primary key AUTOINCREMENT not null,
	TypeName		Text COLLATE NOCASE not null,

	TypeCount		Integer default 0,

	DisplayName		Text default ' ',
	PropertyPrefix          Text default ' ',
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

	Parents			text default ' ',

	unique (TypeName)
);

CREATE TABLE  XesamServiceMapping
(
	ID			Integer primary key AUTOINCREMENT not null,
	XesamTypeName		Text,
	TypeName		Text,

	unique (XesamTypeName, TypeName)
);

CREATE TABLE XesamMetaDataMapping
(
	ID			Integer primary key AUTOINCREMENT not null,
	XesamMetaName		Text,
	MetaName		Text,

	unique (XesamMetaName, MetaName)
);

CREATE TABLE XesamServiceChildren
(
	Parent			Text,
	Child			Text,

	unique (Parent, Child)
);

CREATE TABLE XesamMetaDataChildren
(
	Parent			Text,
	Child			Text,

	unique (Parent, Child)
);

CREATE TABLE XesamServiceLookup
(	
	ID			Integer primary key AUTOINCREMENT not null,
	XesamTypeName		Text,
	TypeName		Text,

	unique (XesamTypeName, TypeName)
);

CREATE TABLE XesamMetaDataLookup
(	
	ID			Integer primary key AUTOINCREMENT not null,
	XesamMetaName		Text,
	MetaName		Text,

	unique (XesamMetaName, MetaName)
);

CREATE TABLE  XesamFileMimes
(
	Mime			Text primary key not null,
	ServiceTypeID		Integer default 0,
	ThumbExec		Text default ' ',
	MetadataExec		Text default ' ',
	FullTextExec		Text default ' '

);

CREATE TABLE  XesamFileMimePrefixes
(
	MimePrefix		Text primary key not null,
	ServiceTypeID		Integer default 0,
	ThumbExec		Text default ' ',
	MetadataExec		Text default ' ',
	FullTextExec		Text default ' '

);
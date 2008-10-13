
/* describes the types of metadata */
CREATE TABLE  MetaDataTypes 
(
	ID	 		Integer primary key AUTOINCREMENT not null,
	MetaName		Text not null  COLLATE NOCASE, 
	DataTypeID		Integer default 1,    /* 0=Keyword, 1=indexable, 2=Clob (compressed indexable text),  3=String, 4=Integer, 5=Double,  6=DateTime, 7=Blob, 8=Struct, 9=ServiceLink */
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

	Unique (MetaName)
);

insert into MetaDataTypes (MetaName) values ('default');

CREATE INDEX  MetaDataTypesIndex1 ON MetaDataTypes (Alias);


/* flattened table to store metadata inter-relationships */
CREATE TABLE  MetaDataChildren
(
	MetaDataID		integer not null,
	ChildID			integer not null,

	primary key (MetaDataID, ChildID)

);


/* for specifying fixed non-extensible metadata group/structs */
CREATE TABLE  MetaDataGroup
(
	MetaDataGroupID		integer not null,
	ChildID			integer not null,

	primary key (MetaDataGroupID, ChildID)

);


/* future-proof table for future addional options specific to a certain metadata type  */
CREATE TABLE MetadataOptions
(
	MetaDataID		Integer not null,
	OptionName		Text not null,
	OptionValue		Text default ' ',

	primary key (MetaDataID, OptionName)
);


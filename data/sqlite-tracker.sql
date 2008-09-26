
CREATE TABLE Options (	
	OptionKey 	Text COLLATE NOCASE not null,	
	OptionValue	Text COLLATE NOCASE
);

insert Into Options (OptionKey, OptionValue) values ('DBVersion', '20');
insert Into Options (OptionKey, OptionValue) values ('Sequence', '1');
insert Into Options (OptionKey, OptionValue) values ('EventSequence', '1');
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


/* provides links from one service entity to another (entities can be in different databases) */
CREATE TABLE  ServiceLinks
(
	ID			Integer primary key AUTOINCREMENT not null,
	MetadataID		Integer not null,
	SourcePath		Text,
	SourceName		Text,
	DestPath		Text,
	DestName		Text
);



/* following two tables are used to backup any user/app defined metadata for indexed/embedded services only */ 
CREATE TABLE  BackupServices
(
	ID            		Integer primary key AUTOINCREMENT not null,
	Path 			Text  not null, 
	Name	 		Text,

	unique (Path, Name)

);


CREATE TABLE  BackupMetaData
(
	ID			Integer primary key  AUTOINCREMENT not null,
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	UserValue		Text
	
	 
);

CREATE INDEX  BackupMetaDataIndex1 ON BackupMetaData (ServiceID, MetaDataID);



CREATE TABLE KeywordImages
(
	Keyword 	Text primary key,
	Image		Text
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

BEGIN TRANSACTION;
ANALYZE sqlite_master;
CREATE TABLE Options (	
	OptionKey 	Text COLLATE NOCASE not null,	
	OptionValue	Text COLLATE NOCASE
);
INSERT INTO "Options" VALUES('DBVersion','20');
INSERT INTO "Options" VALUES('Sequence','1');
INSERT INTO "Options" VALUES('EventSequence','1');
INSERT INTO "Options" VALUES('UpdateCount','0');
INSERT INTO "Options" VALUES('1','IntegrityCheck');
INSERT INTO "Options" VALUES('1','InitialIndex');
CREATE TABLE Volumes
(
	VolumeID 	Integer primary key AUTOINCREMENT not null,
	UDI		Text,
	VolumeName	Text,
	MountPath	Text,
	Enabled		Integer default 0

);
DELETE FROM sqlite_sequence;
INSERT INTO "sqlite_sequence" VALUES('MetaDataTypes',117);
INSERT INTO "sqlite_sequence" VALUES('ServiceTypes',23);
CREATE TABLE ServiceLinks
(
	ID			Integer primary key AUTOINCREMENT not null,
	MetadataID		Integer not null,
	SourcePath		Text,
	SourceName		Text,
	DestPath		Text,
	DestName		Text
);
CREATE TABLE BackupServices
(
	ID            		Integer primary key AUTOINCREMENT not null,
	Path 			Text  not null, 
	Name	 		Text,

	unique (Path, Name)

);
CREATE TABLE BackupMetaData
(
	ID			Integer primary key  AUTOINCREMENT not null,
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	UserValue		Text
	
	 
);
CREATE TABLE KeywordImages
(
	Keyword 	Text primary key,
	Image		Text
);
CREATE TABLE VFolders
(
	Path			Text  not null,
	Name			Text  not null,
	Query			text not null,
	RDF			text,
	Type			Integer default 0,
	active			Integer,

	primary key (Path, Name)

);
CREATE TABLE MetaDataTypes 
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
INSERT INTO "MetaDataTypes" VALUES(1,'default',1,NULL,' ',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(2,'DC:Contributor',1,'Contributor','Contributors to the resource (other than the authors)',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(3,'DC:Coverage',1,'Coverage','The extent or scope of the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(4,'DC:Creator',1,'Author','The authors of the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(5,'DC:Date',6,'Date','Date that something interesting happened to the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(6,'DC:Description',1,'Description','A textual description of the content of the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(7,'DC:Format',0,'Format','The file format(mime) or type used when saving the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(8,'DC:Identifier',1,'Identifier','Unique identifier of the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(9,'DC:Language',1,'Langauge','Language used in the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(10,'DC:Publishers',1,'Publishers','Publishers of the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(11,'DC:Relation',1,'Relationship','Relationships to other resources',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(12,'DC:Rights',1,'Rights','Informal rights statement of the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(13,'DC:Source',1,'Source','Unique identifier of the work from which this resource was derived',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(14,'DC:Subject',1,'Subject','specifies the topic of the content of the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(15,'DC:Keywords',0,'Keywords','Keywords that are used to tag a resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(16,'DC:Title',1,'Title','specifies the topic of the content of the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(17,'DC:Type',1,'Type','specifies the type of the content of the resource',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(18,'User:Rank',4,'Rank','User settable rank or score of the resource',1,0,' ',' ','Rank',1,0,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(19,'User:Keywords',0,'Keywords','User settable keywords which are used to tag a resource',1,0,' ',' ',' ',50,0,1,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(20,'File:Name',1,'Filename','Name of File',1,0,' ',' ','Name',10,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(21,'File:Ext',1,'Extension','File extension',1,0,' ',' ',' ',15,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(22,'File:Path',1,'Path','File Path',1,0,' ',' ','Path',1,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(23,'File:NameDelimited',1,'Keywords','Name of File',1,0,' ',' ',' ',5,1,0,1,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(24,'File:Contents',2,'Contents','File Contents',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(25,'File:Link',1,'Link','File Link',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(26,'File:Mime',0,'Mime Type','File Mime Type',1,0,' ',' ','Mime',10,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(27,'File:Size',4,'Size','File size in bytes',1,0,' ',' ','Size',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(28,'File:License',1,'License','File License Type',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(29,'File:Copyright',1,'Copyright','Copyright owners of the file',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(30,'File:Modified',6,'Modified','Last modified date',1,0,' ',' ','IndexTime',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(31,'File:Accessed',6,'Accessed','Last acessed date',1,0,' ',' ','Accessed',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(32,'File:Other',1,'Other','Other details about a file',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(33,'Audio:Title',1,'Title','Track title',1,0,' ',' ',' ',20,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(34,'Audio:Artist',1,'Artist','Track artist',1,0,' ',' ',' ',15,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(35,'Audio:Album',1,'Title','Track title',1,0,' ',' ',' ',10,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(36,'Audio:Genre',0,'Genre','The type or genre of the music track',1,0,' ',' ',' ',5,1,1,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(37,'Audio:Duration',4,'Duration','The length in seconds of the music track',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(38,'Audio:ReleaseDate',6,'Release date','The date the track was released',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(39,'Audio:AlbumArtist',1,'Album artist',' ',1,0,' ',' ',' ',10,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(40,'Audio:AlbumTrackCount',4,'Album track count','The number of tracks in the album',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(41,'Audio:TrackNo',4,'Track number','The position of the track relative to the others',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(42,'Audio:DiscNo',4,'Disc number','On which disc the track is located',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(43,'Audio:Performer',1,'Performer','The individual or group performing the track',1,0,' ',' ',' ',5,1,1,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(44,'Audio:TrackGain',5,'Track gain','The amount of gain needed for the track',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(45,'Audio:PeakTrackGain',5,'Peak track gain','The peak amount of gain needed for the track',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(46,'Audio:AlbumGain',5,'Album gain','The amount of gain needed for the entire album',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(47,'Audio:AlbumPeakGain',5,'Peak album gain','The peak amount of gain needed for the entire album',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(48,'Audio:Comment',1,'Comments','General purpose comments',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(49,'Audio:Codec',1,'Codec','Codec name',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(50,'Audio:CodecVersion',3,'Codec version','Codec version string',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(51,'Audio:Samplerate',5,'Sample rate','Sample rate of track in Hz',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(52,'Audio:Bitrate',5,'Bitrate','Bitrate in bits/sec',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(53,'Audio:Channels',4,'Channels','The number of channels in the track',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(54,'Audio:LastPlay',6,'Last Played','The date and time the track was last played',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(55,'Audio:PlayCount',4,'Play Count','Number of times the track has been played',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(56,'Audio:DateAdded',6,'Date Added','Date track was first added',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(57,'Audio:Lyrics',1,'Lyrics','Lyrics of the track',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(58,'Audio:MBAlbumID',3,'MusicBrainz album ID','The MusicBrainz album ID for the track',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(59,'Audio:MBArtistID',3,'MusicBrainz artist ID','The MusicBrainz artist ID for the track',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(60,'Audio:MBAlbumArtistID',3,'MusicBrainz album artist ID','The MusicBrainz album artist ID for the track',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(61,'Audio:MBTrackID',3,'MusicBrainz track ID','The MusicBrainz track ID',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(62,'App:Name',1,'Name','Application name',1,0,' ',' ',' ',25,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(63,'App:DisplayName',1,'Display name','Application display name',1,0,' ',' ',' ',10,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(64,'App:GenericName',1,'Generic name','Application generic name',1,0,' ',' ',' ',10,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(65,'App:Comment',1,'Comments','Application comments',1,0,' ',' ',' ',5,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(66,'App:Exec',3,'Name','Application name',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(67,'App:Icon',3,'Icon','Application icon name',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(68,'App:MimeType',0,'Mime type','Application supported mime types',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(69,'App:Categories',0,'Categories','Application categories',1,0,' ',' ',' ',5,1,1,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(70,'Doc:Title',1,'Title','The title of the document',1,0,' ',' ',' ',25,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(71,'Doc:Subject',1,'Subject','The subject or topic of the document',1,0,' ',' ',' ',20,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(72,'Doc:Author',1,'Author','The author of the document',1,0,' ',' ',' ',20,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(73,'Doc:Keywords',0,'Keywords','keywords embedded in the document',1,0,' ',' ',' ',25,1,0,1,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(74,'Doc:Comments',1,'Comments','The comments embedded in the document',1,0,' ',' ',' ',10,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(75,'Doc:PageCount',4,'Page count','Number of pages in the document',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(76,'Doc:WordCount',4,'Word count','Number of words in the document',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(77,'Doc:Created',6,'Created','Date document was created',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(78,'Doc:URL',1,'URL','URL to this Doc',1,0,' ',' ',' ',20,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(79,'Email:Recipient',1,'Recipient','The recepient of an email',1,0,' ',' ',' ',1,1,0,0,1,1,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(80,'Email:Body',1,'Body','The body contents of the email',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(81,'Email:Date',1,'Date','Date email was sent',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(82,'Email:Sender',1,'Sender','The sender of the email',1,0,' ',' ',' ',10,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(83,'Email:Subject',1,'Subject','The subject of the email',1,0,' ',' ',' ',20,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(84,'Email:SentTo',1,'Sent to','The group of people the email was sent to',1,0,' ',' ',' ',10,1,1,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(85,'Email:CC',1,'CC','The CC recipients of the email',1,0,' ',' ',' ',5,1,1,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(86,'Email:Attachments',1,'Attachments','The names of the attachments',1,0,' ',' ',' ',5,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(87,'Email:AttachmentsDelimited',1,'AttachmentsDelimited','The names of the attachments with extra delimiting',1,0,' ',' ',' ',5,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(88,'Image:Title',1,'Title','The title of the image',1,0,' ',' ',' ',10,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(89,'Image:Keywords',1,'Keywords','The keywords embedded in the image',1,0,' ',' ',' ',20,1,0,1,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(90,'Image:Height',1,'Height','Height in pixels of the image',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(91,'Image:Width',1,'Width','Width in pixels of the image',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(92,'Image:Album',1,'Album','The name of the album in which the image resides',1,0,' ',' ',' ',5,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(93,'Image:Date',1,'Created','Date image was created or shot',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(94,'Image:Creator',1,'Creator','The person who created the image',1,0,' ',' ',' ',10,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(95,'Image:Comments',1,'Comments','The comments embedded in the image',1,0,' ',' ',' ',5,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(96,'Image:Description',1,'Description','The description embedded in the image',1,0,' ',' ',' ',5,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(97,'Image:Software',1,'Software','The software used to create the image',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(98,'Image:CameraMake',1,'Camera make','The camera used to create the image',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(99,'Image:CameraModel',1,'Camera model','The model number of the camera used to create the image',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(100,'Image:Orientation',1,'Orientation','The Orientation mode of the image (portrait/landscape)',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(101,'Image:ExposureProgram',1,'Exposure program','The class of the program used by the camera to set exposure when the picture is taken',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(102,'Image:ExposureTime',1,'Exposure time','Exposure time in seconds',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(103,'Image:FNumber',1,'F number','The F number',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(104,'Image:Flash',1,'Flash','Indicates the status of flash when the image was shot (0=off, 1=on)',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(105,'Image:FocalLength',1,'Focal length','The actual focal length of the lens in mm',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(106,'Image:ISOSpeed',1,'ISO speed','Indicates the ISO Speed and ISO Latitude of the camera or input device as specified in ISO 12232.',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(107,'Image:MeteringMode',1,'Metering mode','The metering mode',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(108,'Image:WhiteBalance',1,'White balance','Indicates the white balance mode set when the image was shot (auto/manual)',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(109,'Video:Title',1,'Title','Video title',1,0,' ',' ',' ',20,1,0,0,0,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(110,'Video:Author',1,'Author','Video author',1,0,' ',' ',' ',15,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(111,'Video:Height',4,'Height','The height in pixels',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(112,'Video:Width',4,'Width','The width in pixels',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(113,'Video:Duration',4,'Duration','Duration in number of seconds',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(114,'Video:Comments',1,'Comments','General purpose comments',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(115,'Video:FrameRate',5,'Frame rate','Number of frames per seconds',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(116,'Video:Codec',3,'Codec','Codec used for the video',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
INSERT INTO "MetaDataTypes" VALUES(117,'Video:Bitrate',5,'Bitrate','Bitrate in bits/sec',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ');
CREATE TABLE MetaDataChildren
(
	MetaDataID		integer not null,
	ChildID			integer not null,

	primary key (MetaDataID, ChildID)

);
INSERT INTO "MetaDataChildren" VALUES(15,19);
INSERT INTO "MetaDataChildren" VALUES(8,20);
INSERT INTO "MetaDataChildren" VALUES(8,22);
INSERT INTO "MetaDataChildren" VALUES(11,25);
INSERT INTO "MetaDataChildren" VALUES(7,26);
INSERT INTO "MetaDataChildren" VALUES(12,28);
INSERT INTO "MetaDataChildren" VALUES(12,29);
INSERT INTO "MetaDataChildren" VALUES(5,30);
INSERT INTO "MetaDataChildren" VALUES(5,31);
INSERT INTO "MetaDataChildren" VALUES(16,33);
INSERT INTO "MetaDataChildren" VALUES(4,34);
INSERT INTO "MetaDataChildren" VALUES(17,36);
INSERT INTO "MetaDataChildren" VALUES(5,38);
INSERT INTO "MetaDataChildren" VALUES(4,39);
INSERT INTO "MetaDataChildren" VALUES(2,43);
INSERT INTO "MetaDataChildren" VALUES(6,48);
INSERT INTO "MetaDataChildren" VALUES(5,54);
INSERT INTO "MetaDataChildren" VALUES(8,58);
INSERT INTO "MetaDataChildren" VALUES(8,59);
INSERT INTO "MetaDataChildren" VALUES(8,60);
INSERT INTO "MetaDataChildren" VALUES(8,61);
INSERT INTO "MetaDataChildren" VALUES(16,70);
INSERT INTO "MetaDataChildren" VALUES(14,71);
INSERT INTO "MetaDataChildren" VALUES(4,72);
INSERT INTO "MetaDataChildren" VALUES(15,73);
INSERT INTO "MetaDataChildren" VALUES(6,74);
INSERT INTO "MetaDataChildren" VALUES(5,77);
INSERT INTO "MetaDataChildren" VALUES(5,81);
INSERT INTO "MetaDataChildren" VALUES(4,82);
INSERT INTO "MetaDataChildren" VALUES(14,83);
INSERT INTO "MetaDataChildren" VALUES(79,84);
INSERT INTO "MetaDataChildren" VALUES(79,85);
INSERT INTO "MetaDataChildren" VALUES(16,88);
INSERT INTO "MetaDataChildren" VALUES(15,89);
INSERT INTO "MetaDataChildren" VALUES(5,93);
INSERT INTO "MetaDataChildren" VALUES(4,94);
INSERT INTO "MetaDataChildren" VALUES(6,95);
INSERT INTO "MetaDataChildren" VALUES(6,96);
INSERT INTO "MetaDataChildren" VALUES(16,109);
INSERT INTO "MetaDataChildren" VALUES(4,110);
INSERT INTO "MetaDataChildren" VALUES(6,114);
CREATE TABLE MetaDataGroup
(
	MetaDataGroupID		integer not null,
	ChildID			integer not null,

	primary key (MetaDataGroupID, ChildID)

);
CREATE TABLE MetadataOptions
(
	MetaDataID		Integer not null,
	OptionName		Text not null,
	OptionValue		Text default ' ',

	primary key (MetaDataID, OptionName)
);
CREATE TABLE ServiceTypes
(
	TypeID 			Integer primary key AUTOINCREMENT not null,
	TypeName		Text COLLATE NOCASE not null,

	TypeCount		Integer default 0,

	DisplayName		Text default ' ',
	Parent			Text default ' ',
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
INSERT INTO "ServiceTypes" VALUES(1,'default',0,' ',' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default',' ',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(2,'Files',0,'All Files',' ',1,1,0,0,1,1,1,1,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','icon','All files in the filesystem',0,'system-file-manager',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(3,'Folders',0,'Folders','Files',1,1,0,0,1,1,1,1,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','icon','Folders in the filesystem',0,'folder',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(4,'Documents',0,'Documents','Files',1,1,0,0,1,1,1,1,1,1,1,'File:Contents','Doc:Title','Doc:Author','Doc:Created',' ',' ',' ',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','default','Office and PDF based files',0,'x-office-document',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(5,'WebHistory',0,'Web History',' ',1,1,0,0,1,1,0,0,1,1,1,' ','Doc:Title','Doc:URL','Doc:Keywords','User:Keywords',' ',' ',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','default','Web History',0,'x-office-document',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(6,'Images',0,'Images','Files',1,1,0,0,1,1,1,1,1,0,1,' ','Image:Title','Image:Height','Image:Width','Image:Date','Image:Software','Image:Creator',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','icon','Image based files',0,'image-x-generic',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(7,'Music',0,'Music','Files',1,1,0,0,1,1,1,1,1,0,0,' ','Audio:Title','Audio:Artist','Audio:Album','Audio:Genre','Audio:Duration','Audio:ReleaseDate','Audio:TrackNo','Audio:Bitrate','Audio:PlayCount','Audio:DateAdded','Audio:LastPlay',1,' ',' ',' ',' ','tabular','Music based files',0,'audio-x-generic',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(8,'Videos',0,'Videos','Files',1,1,0,0,1,1,1,1,1,0,1,' ','Video:Title','Video:Author','Video:Height','Video:Width','Video:Duration','Audio:Bitrate',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','icon','Video based files',0,'video-x-generic',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(9,'Text',0,'Text','Files',1,1,0,0,1,1,1,1,0,1,0,'File:Contents',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','default','Text based files',0,'text-x-generic',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(10,'Development',0,'Development','Files',1,1,0,0,1,1,1,1,0,1,0,'File:Contents',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','default','Development and source code files',0,'applications-development',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(11,'Other',0,'Other Files','Files',1,1,0,0,1,1,1,1,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','All other files that do not belong in any other category',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(12,'Emails',0,'Emails',' ',1,1,0,0,1,1,0,0,1,1,1,'Email:Body',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',1,' ','Email:Subject','Email:Sender',' ','default','All Emails',0,'stock_mail-open',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(13,'EvolutionEmails',0,'Evolution Emails','Emails',1,1,0,0,1,1,0,0,1,1,1,'Email:Body','Email:Subject','Email:Sender','Email:Date',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Evolution based emails',0,' ',' ','stdout',' ','evolution "%1"',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(14,'ModestEmails',0,'Modest Emails','Emails',1,1,0,0,1,1,0,0,1,1,1,'Email:Body','Email:Subject','Email:Sender','Email:Date',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Modest based emails',0,' ',' ','stdout',' ','modest-open "%1"',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(15,'ThunderbirdEmails',0,'Thunderbird Emails','Emails',1,1,0,0,1,1,0,0,1,1,1,'Email:Body','Email:Subject','Email:Sender','Email:Date',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Thunderbird based emails',0,' ',' ','stdout',' ','thunderbird -viewtracker "%1"',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(16,'KMailEmails',0,'KMail Emails','Emails',1,1,0,0,1,1,0,0,1,1,1,'Email:Body','Email:Subject','Email:Sender','Email:Date',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','KMail based emails',0,' ',' ','stdout',' ','kmail "%1"',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(17,'EmailAttachments',0,'Email Attachments',' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','default','All files that are attached to an Email',0,'stock_attach',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(18,'EvolutionAttachments',0,'Evolution Email Attachments','EmailAttachments',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','All files that are attached to an Evolution Email',0,'stock_attach',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(19,'ModestAttachments',0,'Modest Email Attachments','EmailAttachments',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','All files that are attached to an Modest Email',0,'stock_attach',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(20,'KMailAttachments',0,'KMail Email Attachments','EmailAttachments',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','All files that are attached to an KMail Email',0,'stock_attach',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(21,'Conversations',0,'Conversations',' ',1,1,0,0,1,1,1,0,0,1,0,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','default','Conversation log files',0,'stock_help-chat',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(22,'GaimConversations',0,'Gaim Conversations','Conversations',1,1,0,0,1,1,1,0,0,1,0,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','All Gaim Conversation logs',0,'stock_help-chat',' ','stdout',' ',' ',' ',' ',' ',' ');
INSERT INTO "ServiceTypes" VALUES(23,'Applications',0,'Applications',' ',1,1,0,0,1,1,1,0,0,0,0,' ','App:DisplayName','App:Exec','App:Icon',' ',' ',' ',' ',' ',' ',' ',' ',1,' ',' ',' ',' ','default','Application files',0,'stock_active',' ','stdout',' ',' ',' ',' ',' ',' ');
CREATE TABLE ServiceTileMetadata
(
	ServiceTypeID		Integer not null,
	MetaName		Text not null,

	primary key (ServiceTypeID, MetaName)
);
INSERT INTO "ServiceTileMetadata" VALUES(4,'Doc:Title');
INSERT INTO "ServiceTileMetadata" VALUES(4,'Doc:Subject');
INSERT INTO "ServiceTileMetadata" VALUES(4,'Doc:Author');
INSERT INTO "ServiceTileMetadata" VALUES(4,'Doc:Created');
INSERT INTO "ServiceTileMetadata" VALUES(4,'Doc:PageCount');
INSERT INTO "ServiceTileMetadata" VALUES(4,'File:Size');
INSERT INTO "ServiceTileMetadata" VALUES(5,'Doc:Title');
INSERT INTO "ServiceTileMetadata" VALUES(5,'Doc:URL');
INSERT INTO "ServiceTileMetadata" VALUES(5,'Doc:Subject');
INSERT INTO "ServiceTileMetadata" VALUES(5,'Doc:Author');
INSERT INTO "ServiceTileMetadata" VALUES(5,'Doc:Created');
INSERT INTO "ServiceTileMetadata" VALUES(5,'Doc:PageCount');
INSERT INTO "ServiceTileMetadata" VALUES(5,'File:Size');
INSERT INTO "ServiceTileMetadata" VALUES(6,'Image:Title');
INSERT INTO "ServiceTileMetadata" VALUES(6,'Image:Height');
INSERT INTO "ServiceTileMetadata" VALUES(6,'Image:Width');
INSERT INTO "ServiceTileMetadata" VALUES(6,'Image:Date');
INSERT INTO "ServiceTileMetadata" VALUES(6,'Image:Creator');
INSERT INTO "ServiceTileMetadata" VALUES(6,'Image:Software');
INSERT INTO "ServiceTileMetadata" VALUES(6,'Image:Comments');
INSERT INTO "ServiceTileMetadata" VALUES(7,'Audio:Title');
INSERT INTO "ServiceTileMetadata" VALUES(7,'Audio:Artist');
INSERT INTO "ServiceTileMetadata" VALUES(7,'Audio:Album');
INSERT INTO "ServiceTileMetadata" VALUES(7,'Audio:Genre');
INSERT INTO "ServiceTileMetadata" VALUES(7,'Audio:Duration');
INSERT INTO "ServiceTileMetadata" VALUES(7,'Audio:ReleaseDate');
INSERT INTO "ServiceTileMetadata" VALUES(8,'Video:Title');
INSERT INTO "ServiceTileMetadata" VALUES(8,'Video:Author');
INSERT INTO "ServiceTileMetadata" VALUES(8,'Video:Height');
INSERT INTO "ServiceTileMetadata" VALUES(8,'Video:Width');
INSERT INTO "ServiceTileMetadata" VALUES(8,'Video:Duration');
INSERT INTO "ServiceTileMetadata" VALUES(8,'Video:Bitrate');
INSERT INTO "ServiceTileMetadata" VALUES(12,'Email:Sender');
INSERT INTO "ServiceTileMetadata" VALUES(12,'Email:Subject');
INSERT INTO "ServiceTileMetadata" VALUES(12,'Email:Date');
INSERT INTO "ServiceTileMetadata" VALUES(12,'Email:SentTo');
INSERT INTO "ServiceTileMetadata" VALUES(12,'Email:CC');
INSERT INTO "ServiceTileMetadata" VALUES(12,'Email:Attachments');
INSERT INTO "ServiceTileMetadata" VALUES(13,'Email:Sender');
INSERT INTO "ServiceTileMetadata" VALUES(13,'Email:Subject');
INSERT INTO "ServiceTileMetadata" VALUES(13,'Email:Date');
INSERT INTO "ServiceTileMetadata" VALUES(13,'Email:SentTo');
INSERT INTO "ServiceTileMetadata" VALUES(13,'Email:CC');
INSERT INTO "ServiceTileMetadata" VALUES(13,'Email:Attachments');
INSERT INTO "ServiceTileMetadata" VALUES(14,'Email:Sender');
INSERT INTO "ServiceTileMetadata" VALUES(14,'Email:Subject');
INSERT INTO "ServiceTileMetadata" VALUES(14,'Email:Date');
INSERT INTO "ServiceTileMetadata" VALUES(14,'Email:SentTo');
INSERT INTO "ServiceTileMetadata" VALUES(14,'Email:CC');
INSERT INTO "ServiceTileMetadata" VALUES(14,'Email:Attachments');
INSERT INTO "ServiceTileMetadata" VALUES(15,'Email:Sender');
INSERT INTO "ServiceTileMetadata" VALUES(15,'Email:Subject');
INSERT INTO "ServiceTileMetadata" VALUES(15,'Email:Date');
INSERT INTO "ServiceTileMetadata" VALUES(15,'Email:SentTo');
INSERT INTO "ServiceTileMetadata" VALUES(15,'Email:CC');
INSERT INTO "ServiceTileMetadata" VALUES(15,'Email:Attachments');
INSERT INTO "ServiceTileMetadata" VALUES(16,'Email:Sender');
INSERT INTO "ServiceTileMetadata" VALUES(16,'Email:Subject');
INSERT INTO "ServiceTileMetadata" VALUES(16,'Email:Date');
INSERT INTO "ServiceTileMetadata" VALUES(16,'Email:SentTo');
INSERT INTO "ServiceTileMetadata" VALUES(16,'Email:CC');
INSERT INTO "ServiceTileMetadata" VALUES(16,'Email:Attachments');
INSERT INTO "ServiceTileMetadata" VALUES(23,'App:GenericName');
INSERT INTO "ServiceTileMetadata" VALUES(23,'AppComment');
INSERT INTO "ServiceTileMetadata" VALUES(23,'App:Categories');
CREATE TABLE ServiceTabularMetadata
(
	ServiceTypeID		Integer not null,
	MetaName		Text not null,

	primary key (ServiceTypeID, MetaName)
);
INSERT INTO "ServiceTabularMetadata" VALUES(4,'File:Name');
INSERT INTO "ServiceTabularMetadata" VALUES(4,'File:Mime');
INSERT INTO "ServiceTabularMetadata" VALUES(4,'Doc:Title');
INSERT INTO "ServiceTabularMetadata" VALUES(4,'Doc:Author');
INSERT INTO "ServiceTabularMetadata" VALUES(4,'File:Size');
INSERT INTO "ServiceTabularMetadata" VALUES(4,'File:Modified');
INSERT INTO "ServiceTabularMetadata" VALUES(4,'Doc:Created');
INSERT INTO "ServiceTabularMetadata" VALUES(5,'File:Name');
INSERT INTO "ServiceTabularMetadata" VALUES(5,'File:Mime');
INSERT INTO "ServiceTabularMetadata" VALUES(5,'Doc:Title');
INSERT INTO "ServiceTabularMetadata" VALUES(5,'Doc:URL');
INSERT INTO "ServiceTabularMetadata" VALUES(5,'Doc:Author');
INSERT INTO "ServiceTabularMetadata" VALUES(5,'File:Size');
INSERT INTO "ServiceTabularMetadata" VALUES(5,'File:Modified');
INSERT INTO "ServiceTabularMetadata" VALUES(5,'Doc:Created');
INSERT INTO "ServiceTabularMetadata" VALUES(6,'File:Name');
INSERT INTO "ServiceTabularMetadata" VALUES(6,'Image:Height');
INSERT INTO "ServiceTabularMetadata" VALUES(6,'Image:Width');
INSERT INTO "ServiceTabularMetadata" VALUES(6,'Image:Date');
INSERT INTO "ServiceTabularMetadata" VALUES(6,'File:Modified');
INSERT INTO "ServiceTabularMetadata" VALUES(6,'Image:Creator');
INSERT INTO "ServiceTabularMetadata" VALUES(6,'Image:Software');
INSERT INTO "ServiceTabularMetadata" VALUES(7,'Audio:Title');
INSERT INTO "ServiceTabularMetadata" VALUES(7,'Audio:Artist');
INSERT INTO "ServiceTabularMetadata" VALUES(7,'Audio:Album');
INSERT INTO "ServiceTabularMetadata" VALUES(7,'Audio:Genre');
INSERT INTO "ServiceTabularMetadata" VALUES(7,'Audio:Duration');
INSERT INTO "ServiceTabularMetadata" VALUES(7,'Audio:ReleaseDate');
INSERT INTO "ServiceTabularMetadata" VALUES(8,'File:Name');
INSERT INTO "ServiceTabularMetadata" VALUES(8,'Video:Title');
INSERT INTO "ServiceTabularMetadata" VALUES(8,'Video:Author');
INSERT INTO "ServiceTabularMetadata" VALUES(8,'Video:Height');
INSERT INTO "ServiceTabularMetadata" VALUES(8,'Video:Width');
INSERT INTO "ServiceTabularMetadata" VALUES(8,'Video:Duration');
INSERT INTO "ServiceTabularMetadata" VALUES(8,'Video:Bitrate');
INSERT INTO "ServiceTabularMetadata" VALUES(12,'Email:Sender');
INSERT INTO "ServiceTabularMetadata" VALUES(12,'Email:Subject');
INSERT INTO "ServiceTabularMetadata" VALUES(12,'Email:Date');
INSERT INTO "ServiceTabularMetadata" VALUES(13,'Email:Sender');
INSERT INTO "ServiceTabularMetadata" VALUES(13,'Email:Subject');
INSERT INTO "ServiceTabularMetadata" VALUES(13,'Email:Date');
INSERT INTO "ServiceTabularMetadata" VALUES(14,'Email:Sender');
INSERT INTO "ServiceTabularMetadata" VALUES(14,'Email:Subject');
INSERT INTO "ServiceTabularMetadata" VALUES(14,'Email:Date');
INSERT INTO "ServiceTabularMetadata" VALUES(15,'Email:Sender');
INSERT INTO "ServiceTabularMetadata" VALUES(15,'Email:Subject');
INSERT INTO "ServiceTabularMetadata" VALUES(15,'Email:Date');
INSERT INTO "ServiceTabularMetadata" VALUES(16,'Email:Sender');
INSERT INTO "ServiceTabularMetadata" VALUES(16,'Email:Subject');
INSERT INTO "ServiceTabularMetadata" VALUES(16,'Email:Date');
CREATE TABLE ServiceTypeOptions
(
	ServiceTypeID		Integer not null,
	OptionName		Text not null,
	OptionValue		Text default ' ',

	primary key (ServiceTypeID, OptionName)
);
CREATE TABLE FileMimes
(
	Mime			Text primary key not null,
	ServiceTypeID		Integer default 0,
	ThumbExec		Text default ' ',
	MetadataExec		Text default ' ',
	FullTextExec		Text default ' '

);
INSERT INTO "FileMimes" VALUES('application/rtf',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/richtext',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/msword',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/pdf',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/postscript',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-dvi',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/vnd.ms-excel',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('vnd.ms-powerpoint',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-abiword',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/html',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/sgml',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-tex',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-mswrite',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-applix-word',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/docbook+xml',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-kword',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-kword-crypt',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-lyx',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/vnd.lotus-1-2-3',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-applix-spreadsheet',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-gnumeric',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-kspread',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-kspread-crypt',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-quattropro',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-sc',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-siag',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-magicpoint',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-kpresenter',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/illustrator',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/vnd.corel-draw',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/vnd.stardivision.draw',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/vnd.oasis.opendocument.graphics',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-dia-diagram',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-karbon',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-killustrator',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-kivio',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-kontour',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-wpg',4,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/vnd.oasis.opendocument.image',6,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-krita',6,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/ogg',7,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/plain',9,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-authors',9,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-changelog',9,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-copying',9,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-credits',9,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-install',9,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-readme',9,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-perl',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-shellscript',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-php',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-java',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-javascript',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-glade',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-csh',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-class-file',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-awk',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-asp',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-ruby',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('application/x-m4',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-m4',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-c++',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-adasrc',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-c',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-c++hdr',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-chdr',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-csharp',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-c++src',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-csrc',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-dcl',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-dsrc',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-emacs-lisp',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-fortran',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-haskell',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-literate-haskell',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-java',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-java-source" ,text/x-makefile',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-objcsrc',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-pascal',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-patch',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-python',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-scheme',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-sql',10,' ',' ',' ');
INSERT INTO "FileMimes" VALUES('text/x-tcl',10,' ',' ',' ');
CREATE TABLE FileMimePrefixes
(
	MimePrefix		Text primary key not null,
	ServiceTypeID		Integer default 0,
	ThumbExec		Text default ' ',
	MetadataExec		Text default ' ',
	FullTextExec		Text default ' '

);
INSERT INTO "FileMimePrefixes" VALUES('application/vnd.oasis.opendocument',4,' ',' ',' ');
INSERT INTO "FileMimePrefixes" VALUES('application/vnd.sun.xml',4,' ',' ',' ');
INSERT INTO "FileMimePrefixes" VALUES('application/vnd.stardivision',4,' ',' ',' ');
INSERT INTO "FileMimePrefixes" VALUES('image/',6,' ',' ',' ');
INSERT INTO "FileMimePrefixes" VALUES('audio/',7,' ',' ',' ');
INSERT INTO "FileMimePrefixes" VALUES('video/',8,' ',' ',' ');
CREATE INDEX BackupMetaDataIndex1 ON BackupMetaData (ServiceID, MetaDataID);
CREATE INDEX MetaDataTypesIndex1 ON MetaDataTypes (Alias);
COMMIT;

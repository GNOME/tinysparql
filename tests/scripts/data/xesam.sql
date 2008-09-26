BEGIN TRANSACTION;
ANALYZE sqlite_master;
CREATE TABLE XesamMetaDataTypes 
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
INSERT INTO "XesamMetaDataTypes" VALUES(1,'xesam:35mmEquivalent',5,NULL,'Photo metering mode',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(2,'xesam:acl',3,NULL,'File access control list',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Filelike',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(3,'xesam:album',3,NULL,'Media album',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(4,'xesam:albumArtist',3,NULL,'Music album artist',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(5,'xesam:albumGain',5,NULL,'Gain adjustment of album',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(6,'xesam:albumPeakGain',5,NULL,'Peak gain adjustment of album',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(7,'xesam:albumTrackCount',4,NULL,'Album track count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(8,'xesam:aperture',5,NULL,'Photo aperture',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(9,'xesam:artist',3,NULL,'Music artist',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(10,'xesam:asText',3,NULL,'Content plain-text representation for indexing purposes',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(11,'xesam:aspectRatio',3,NULL,'Visual content aspect ratio',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(12,'xesam:attachmentEncoding',3,NULL,'Email attachment encoding(base64,utf-7, etc)',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:EmailAttachment',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(13,'xesam:audioBPM',4,NULL,'Beats per minute',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(14,'xesam:audioBitrate',4,NULL,'Audio Bitrate',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(15,'xesam:audioChannels',3,NULL,'Audio channels',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(16,'xesam:audioCodec',3,NULL,'Audio codec',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(17,'xesam:audioCodecType',3,NULL,'Audio codec type',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(18,'xesam:audioSampleBitDepth',4,NULL,'Audio sample data bit depth',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(19,'xesam:audioSampleCount',4,NULL,'Audio sample count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(20,'xesam:audioSampleDataType',3,NULL,'Audio sample data type',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(21,'xesam:audioSampleRate',5,NULL,'Audio sample rate',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(22,'xesam:author',3,NULL,'Content author. Primary contributor.',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(23,'xesam:autoRating',0,NULL,'Rating of the object provided automatically by software, inferred from user behavior or other indirect indicators.',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(24,'xesam:baseRevisionID',3,NULL,'RevisionID on which a revision-controlled file is based',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:RevisionControlledFile',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(25,'xesam:bcc',3,NULL,'BCC:',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Email',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(26,'xesam:birthDate',6,NULL,'Contact birthDate',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(27,'xesam:blogContactURL',3,NULL,'Contact blog URL',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(28,'xesam:cameraManufacturer',3,NULL,'Photo camera manufacturer',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(29,'xesam:cameraModel',3,NULL,'Photo camera model',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(30,'xesam:cc',3,NULL,'CC:',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Email',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(31,'xesam:ccdWidth',5,NULL,'Photo CCD Width',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(32,'xesam:cellPhoneNumber',3,NULL,'Contact cell phone number',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(33,'xesam:changeCommitTime',6,NULL,'Time of the last change to the base file in the repository(preceding the baseRevisionID?)',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:RevisionControlledFile',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(34,'xesam:changeCommitter',3,NULL,'Who made the last change to the base file in the repository(preceding the baseRevisionID?)',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:RevisionControlledFile',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(35,'xesam:characterCount',4,NULL,'Text character count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Text',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(36,'xesam:charset',3,NULL,'Content charset encoding',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(37,'xesam:chatRoom',3,NULL,'Chatroom this message belongs to',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:IMMessage',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(38,'xesam:colorCount',4,NULL,'Visual content color count for palettes',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(39,'xesam:colorSpace',3,NULL,'Visual content color space(RGB, CMYK etc)',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(40,'xesam:columnCount',4,NULL,'Spreadsheet column count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Spreadsheet',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(41,'xesam:comment',3,NULL,'Object comment',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:DataObject',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(42,'xesam:commentCharacterCount',4,NULL,'Source code comment character count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:SourceCode',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(43,'xesam:commitDiff',0,NULL,'The diff of the content and the base file',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:RevisionControlledFile',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(44,'xesam:communicationChannel',3,NULL,'Message communication channel like chatroom name or mailing list',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Message',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(45,'xesam:composer',3,NULL,'Music composer',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(46,'xesam:compressionAlgorithm',3,NULL,'Compression algorithm for archivers which support several',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:ArchivedFile',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(47,'xesam:compressionLevel',3,NULL,'Level of compression. How much effort was spent towards achieving maximal compression ratio.',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:ArchivedFile;xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(48,'xesam:conflicts',3,NULL,'Software conflicts with',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:SoftwarePackage',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(49,'xesam:contactMedium',3,NULL,'Generic contact medium',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(50,'xesam:contactNick',3,NULL,'Contact nick',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(51,'xesam:contactURL',3,NULL,'Contact URL',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(52,'xesam:contains',3,NULL,'Containment relation',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(53,'xesam:contentComment',3,NULL,'Content comment',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(54,'xesam:contentCreated',6,NULL,'Content creation time',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(55,'xesam:contentKeyword',3,NULL,'Content keyword/tag',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(56,'xesam:contentModified',6,NULL,'Content last modification time',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(57,'xesam:contentType',3,NULL,'Email content mime type/charset',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Email',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(58,'xesam:contributor',3,NULL,'Content contributor. Secondary contributor.',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(59,'xesam:copyright',3,NULL,'Content copyright',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(60,'xesam:creator',0,NULL,'Abstract content creator. Use children',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(61,'xesam:definesClass',3,NULL,'Source code defines class',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:SourceCode',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(62,'xesam:definesFunction',3,NULL,'Source code defines function',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:SourceCode',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(63,'xesam:definesGlobalVariable',3,NULL,'Source code defines global variable',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:SourceCode',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(64,'xesam:deletionTime',6,NULL,'File deletion time',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:DeletedFile',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(65,'xesam:depends',3,NULL,'Dependency relation',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(66,'xesam:derivedFrom',3,NULL,'Links to the original content from which this content is derived',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(67,'xesam:description',3,NULL,'Content description. Description of content an order of magnitude more elaborate than Title',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(68,'xesam:discNumber',4,NULL,'Audio cd number',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(69,'xesam:disclaimer',3,NULL,'Content disclaimer',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(70,'xesam:documentCategory',3,NULL,'Document category: book, article, flyer, pamphlet whatever',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Document',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(71,'xesam:emailAddress',3,NULL,'Contact email address',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(72,'xesam:exposureBias',3,NULL,'Photo exposure bias',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(73,'xesam:exposureProgram',3,NULL,'Photo exposure program',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(74,'xesam:exposureTime',6,NULL,'Photo exposure time',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(75,'xesam:familyName',3,NULL,'Person family name',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(76,'xesam:faxPhoneNumber',3,NULL,'Contact fax phone number',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(77,'xesam:fileExtension',3,NULL,'File extension',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Filelike',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(78,'xesam:fileSystemType',3,NULL,'File system type e.g. ext3',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:FileSystem',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(79,'xesam:fingerprint',0,NULL,'Content fingerprint: a small ID calculated from content byte stream, aimed at uniquely identifying the content. Abstract.',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(80,'xesam:firstUsed',6,NULL,'When the content was used for the first time',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(81,'xesam:flashUsed',3,NULL,'Photo flash used',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(82,'xesam:focalLength',5,NULL,'Photo focal length',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(83,'xesam:focusDistance',5,NULL,'Photo focus distance',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(84,'xesam:formatSubtype',3,NULL,'Format subtype. Use to indicate format extensions/specifics',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(85,'xesam:frameCount',4,NULL,'Visual content frame count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(86,'xesam:frameRate',5,NULL,'Visual content frame rate',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(87,'xesam:freeSpace',4,NULL,'File system free space',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:FileSystem',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(88,'xesam:gender',3,NULL,'Contact gender',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(89,'xesam:generator',3,NULL,'Software used to generate the content byte stream',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(90,'xesam:generatorOptions',3,NULL,'Generator software options',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(91,'xesam:genre',3,NULL,'Media genre',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(92,'xesam:givenName',3,NULL,'Person given name',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(93,'xesam:group',3,NULL,'File group',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Filelike',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(94,'xesam:height',4,NULL,'Visual content height',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(95,'xesam:homeEmailAddress',3,NULL,'Contact home email address',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(96,'xesam:homePhoneNumber',3,NULL,'Contact home phone number',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(97,'xesam:homePostalAddress',3,NULL,'Contact home address',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(98,'xesam:homepageContactURL',3,NULL,'Contact homepage URL',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(99,'xesam:honorificPrefix',3,NULL,'Person honorific name prefix',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(100,'xesam:honorificSuffix',3,NULL,'Person honorific name suffix',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(101,'xesam:horizontalResolution',4,NULL,'Visual content horizontal resolution',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(102,'xesam:id',3,NULL,'Content ID',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(103,'xesam:imContactMedium',3,NULL,'Generic IM contact medium',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(104,'xesam:inReplyTo',3,NULL,'In-Reply-To:',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Email',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(105,'xesam:interest',3,NULL,'Contact interests/hobbies',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(106,'xesam:interlaceMode',3,NULL,'Visual content interlace mode',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(107,'xesam:ircContactMedium',3,NULL,'Contact IRC ID@server',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(108,'xesam:isContentEncrypted',4,NULL,' ',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(109,'xesam:isEncrypted',0,NULL,'Is Object or part of it encrypted?',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:DataObject',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(110,'xesam:isImportant',4,NULL,'Is the message important',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:MessageboxMessage',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(111,'xesam:isInProgress',4,NULL,'Is the message in progress',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:MessageboxMessage',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(112,'xesam:isPublicChannel',4,NULL,'Is channel public?',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:CommunicationChannel',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(113,'xesam:isRead',4,NULL,'Is the message read',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:MessageboxMessage',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(114,'xesam:isSourceEncrypted',4,NULL,'Is archived file password-protected?',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:ArchivedFile',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(115,'xesam:isoEquivalent',3,NULL,'Photo ISO equivalent',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(116,'xesam:jabberContactMedium',3,NULL,'Contact Jabber ID',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(117,'xesam:keyword',3,NULL,'Object keyword/tag',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:DataObject',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(118,'xesam:knows',3,NULL,'FOAF:knows relation. Points to a contact known by this contact.',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(119,'xesam:language',3,NULL,'Content language',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(120,'xesam:lastRefreshed',6,NULL,'Last time the resource info was refreshed',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:RemoteResource',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(121,'xesam:lastUsed',6,NULL,'When the content was last used. Different from last access as this only accounts usage by the user e.g. playing a song as opposed to apps scanning the HD',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(122,'xesam:legal',3,NULL,'Abstract content legal notice.',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(123,'xesam:license',3,NULL,'Content license',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(124,'xesam:licenseType',3,NULL,'Content license type',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(125,'xesam:lineCount',4,NULL,'Text line count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Text',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(126,'xesam:links',3,NULL,'Linking/mention relation',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(127,'xesam:localRevision',3,NULL,'Local revision number. An automatically generated ID that is changed everytime the generator software/revisioning system deems the content has changed.',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(128,'xesam:lyricist',3,NULL,'Music lyricist',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(129,'xesam:mailingList',3,NULL,'Mailing list this message belongs to',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:MailingListEmail',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(130,'xesam:mailingPostalAddress',3,NULL,'Contact mailing address',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(131,'xesam:maintainer',3,NULL,'Content maintainer.',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(132,'xesam:markupCharacterCount',4,NULL,'XML markup character count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:XML',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(133,'xesam:md5Hash',0,NULL,'MD5 hash',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(134,'xesam:mediaBitrate',0,NULL,'Media bitrate',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(135,'xesam:mediaCodec',3,NULL,'Media codec',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(136,'xesam:mediaCodecType',3,NULL,'Media codec type: lossless, CBR, ABR, VBR',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(137,'xesam:mediaDuration',0,NULL,'Media duration',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(138,'xesam:mergeConflict',4,NULL,' ',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:RevisionControlledFile',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(139,'xesam:meteringMode',3,NULL,'Photo metering mode',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(140,'xesam:mimeType',3,NULL,'Content mime-type',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(141,'xesam:mountPoint',3,NULL,'File system mount point',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:FileSystem',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(142,'xesam:name',3,NULL,'Name provided by container',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(143,'xesam:newsGroup',3,NULL,'News group this message belongs to',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:NewsGroupEmail',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(144,'xesam:occupiedSpace',4,NULL,'File system occupied space',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:FileSystem',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(145,'xesam:orientation',3,NULL,'Photo orientation',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(146,'xesam:originURL',0,NULL,'Origin URL, e.g. where the file had been downloaded from',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(147,'xesam:originalLocation',3,NULL,'Deleted file original location',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:DeletedFile',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(148,'xesam:otherName',3,NULL,'Person other name',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(149,'xesam:owner',3,NULL,'File owner',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Filelike',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(150,'xesam:pageCount',4,NULL,'Document page count. Slide count for presentations',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Document',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(151,'xesam:paragrapCount',4,NULL,'Document paragraph count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Document',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(152,'xesam:performer',3,NULL,'Music performer',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(153,'xesam:permissions',3,NULL,'File permissions',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Filelike',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(154,'xesam:personPhoto',3,NULL,'Contact photo/avatar',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(155,'xesam:phoneNumber',3,NULL,'Contact phone number',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(156,'xesam:physicalAddress',3,NULL,'Contact postal address',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(157,'xesam:pixelDataBitDepth',4,NULL,'Visual content pixel data bit depth',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(158,'xesam:pixelDataType',3,NULL,'Visual content pixel data type',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(159,'xesam:primaryRecipient',3,NULL,'Message primary recipient',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Message',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(160,'xesam:programmingLanguage',3,NULL,'Source code programming language',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:SourceCode',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(161,'xesam:receptionTime',6,NULL,'Message reception time',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Message',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(162,'xesam:recipient',3,NULL,'Message recipient',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Message',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(163,'xesam:related',3,NULL,'Abstract content relation. Use children',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(164,'xesam:remotePassword',3,NULL,'Remote resource user password',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:RemoteResource',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(165,'xesam:remotePort',4,NULL,'Server port',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:RemoteResource',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(166,'xesam:remoteServer',3,NULL,'The server hosting the remote resource',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:RemoteResource',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(167,'xesam:remoteUser',3,NULL,'Remote resource user name',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:RemoteResource',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(168,'xesam:replyTo',3,NULL,'ReplyTo:',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Email',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(169,'xesam:rowCount',4,NULL,'Spreadsheet row count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Spreadsheet',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(170,'xesam:rssFeed',3,NULL,'RSS feed that provided the message',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:RSSMessage',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(171,'xesam:sampleBitDepth',0,NULL,'Media sample data bit depth: 8, 16, 24, 32 etc',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(172,'xesam:sampleConfiguration',0,NULL,'Media sample configuration/arrangement of components',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(173,'xesam:sampleDataType',3,NULL,'Media sample data type: int, float',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(174,'xesam:secondaryRecipient',3,NULL,'Message secondary recipient',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Message',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(175,'xesam:seenAttachedAs',3,NULL,'Name of block device seen to contain the Content when it was online.',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:OfflineMedia',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(176,'xesam:setCount',0,NULL,'Media set count. Sample count for audio(set=one sample per channel), frame count for video',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(177,'xesam:setRate',0,NULL,'Media set rate. Sample rate for audio(set=one sample per channel), FPS for video',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(178,'xesam:sha1Hash',0,NULL,'SHA1 hash',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(179,'xesam:size',4,NULL,'Content/data size in bytes. See also storageSize',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(180,'xesam:sourceCreated',6,NULL,'Local copy creation time',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(181,'xesam:sourceModified',6,NULL,'Local copy modification time',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(182,'xesam:storedSize',4,NULL,'Actual space occupied by the object in the source storage. e.g. compressed file size in archive',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(183,'xesam:subject',3,NULL,'Content subject. The shortest possible description of content',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(184,'xesam:summary',3,NULL,'Content summary. Description of content an order of magnitude more elaborate than Description',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(185,'xesam:supercedes',3,NULL,'Software supercedes',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:SoftwarePackage',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(186,'xesam:targetQuality',3,NULL,'The desired level of quality loss of lossy compression',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(187,'xesam:title',3,NULL,'Content title. Description of content an order of magnitude more elaborate than Subject',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(188,'xesam:to',3,NULL,'To:',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Email',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(189,'xesam:totalSpace',4,NULL,'File system total usable space, unlike size, which is the byte size of the entire filesystem including overhead.',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:FileSystem',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(190,'xesam:totalUncompressedSize',4,NULL,'Archive total uncompressed size',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Archive',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(191,'xesam:trackGain',5,NULL,'Gain adjustment of track',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(192,'xesam:trackNumber',4,NULL,'Audio track number',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(193,'xesam:trackPeakGain',5,NULL,'Peak gain adjustment of track',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(194,'xesam:url',3,NULL,'URL to access the content',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(195,'xesam:useCount',4,NULL,'How many times the content was used. Only usage by the user(not general software access) counts.',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(196,'xesam:userComment',3,NULL,'User-provided comment',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(197,'xesam:userKeyword',3,NULL,'User-provided keywords',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(198,'xesam:userRating',0,NULL,'User-provided rating of the object',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Source',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(199,'xesam:usesNamespace',3,NULL,'Namespace referenced by the XML',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:XML',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(200,'xesam:version',3,NULL,'Content version',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Content',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(201,'xesam:verticalResolution',4,NULL,'Visual content vertical resolution',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(202,'xesam:videoBitrate',4,NULL,'Video Bitrate',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Video',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(203,'xesam:videoCodec',3,NULL,'Video codec',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Video',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(204,'xesam:videoCodecType',3,NULL,'Video codec type',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Video',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(205,'xesam:whiteBalance',3,NULL,'Photo white balance',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Photo',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(206,'xesam:width',4,NULL,'Visual content width',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Visual',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(207,'xesam:wordCount',4,NULL,'Text word count',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Text',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(208,'xesam:workEmailAddress',3,NULL,'Contact work email address',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(209,'xesam:workPhoneNumber',3,NULL,'Contact work phone number',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(210,'xesam:workPostalAddress',3,NULL,'Contact work address',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Person',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(211,'xesam:actionAccessClassification',3,NULL,'PIM entry access classification',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(212,'xesam:actionContact',3,NULL,'PIM entry contact',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:FreeBusy;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(213,'xesam:actionDuration',3,NULL,'PIM entry action duration',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Alarm;xesam:Event;xesam:FreeBusy;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(214,'xesam:actionEnd',3,NULL,'PIM entry action end',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:FreeBusy',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(215,'xesam:actionExceptionDate',3,NULL,'PIM entry exception date',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(216,'xesam:actionExceptionRule',3,NULL,'PIM entry exception rule',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(217,'xesam:actionLocation',3,NULL,'PIM entry location',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(218,'xesam:actionOrganizer',3,NULL,'PIM entry organizer',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:FreeBusy;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(219,'xesam:actionPriority',3,NULL,'PIM entry priority',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(220,'xesam:actionRecurrenceDate',3,NULL,'PIM entry recurrence date',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(221,'xesam:actionRecurrenceID',3,NULL,'PIM entry recurrence ID',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(222,'xesam:actionRecurrenceRule',3,NULL,'PIM entry recurrence rule',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(223,'xesam:actionResources',3,NULL,'PIM activity has alarm',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(224,'xesam:actionStart',3,NULL,'PIM entry action start',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:FreeBusy;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(225,'xesam:actionStatus',3,NULL,'PIM entry status',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(226,'xesam:actionTrigger',3,NULL,'PIM entry action trigger',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Alarm;xesam:Event;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(227,'xesam:actionURL',3,NULL,'PIM entry URL',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event;xesam:FreeBusy;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(228,'xesam:aimContactMedium',3,NULL,'Contact AIM ID',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(229,'xesam:alarmAction',3,NULL,'Alarm action',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Alarm',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(230,'xesam:alarmRepeat',3,NULL,'Alarm repeat',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Alarm',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(231,'xesam:applicationDesktopEntryExec',3,NULL,'Command to execute, possibly with arguments',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:ApplicationDesktopEntry',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(232,'xesam:attendee',3,NULL,'PIM entry attendee',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Alarm;xesam:Event;xesam:FreeBusy;xesam:Journal;xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(233,'xesam:desktopEntryIcon',3,NULL,'Desktop entry Icon field value conforming to http://freedesktop.org/wiki/Standards/icon-theme-spec',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:DesktopEntry',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(234,'xesam:desktopMenuCategory',3,NULL,'Category in which the entry should be shown in the desktop menu. http://www.freedesktop.org/Standards/menu-spec',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:DesktopEntry',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(235,'xesam:eventEnd',3,NULL,'Event end time',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(236,'xesam:eventLocation',3,NULL,'Event location',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(237,'xesam:eventStart',3,NULL,'Event start time',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(238,'xesam:eventTransparrent',3,NULL,'Is event transparrent(makes person busy)',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Event',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(239,'xesam:icqContactMedium',3,NULL,'Contact ICQ ID',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(240,'xesam:imdbId',0,NULL,'IMDB.com video ID',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Video',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(241,'xesam:isrc',0,NULL,'International Standard Recording Code',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Media',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(242,'xesam:msnContactMedium',3,NULL,'Contact MSN ID',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(243,'xesam:musicBrainzAlbumArtistID',3,NULL,'MusicBrainz album artist ID in UUID format',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(244,'xesam:musicBrainzAlbumID',3,NULL,'MusicBrainz album ID in UUID format',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(245,'xesam:musicBrainzArtistID',3,NULL,'MusicBrainz artist ID in UUID format',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(246,'xesam:musicBrainzFingerprint',0,NULL,'MusicBrainz track fingerprint',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(247,'xesam:musicBrainzTrackID',3,NULL,'MusicBrainz track ID in UUID format',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Audio',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(248,'xesam:skypeContactMedium',3,NULL,'Contact Skype ID',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(249,'xesam:supportedMimeType',3,NULL,'The MIME type supported by this application',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:ApplicationDesktopEntry',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(250,'xesam:taskCompleted',0,NULL,'Is task completed?',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(251,'xesam:taskDue',0,NULL,'Task due date/time',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(252,'xesam:taskPercentComplete',0,NULL,'Task completeness',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:Task',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(253,'xesam:yahooContactMedium',3,NULL,'Contact Yahoo ID',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:Contact',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(254,'xesam:contentCategory',3,NULL,'Identifier of content category',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:DataObject',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(255,'xesam:sourceCategory',3,NULL,'Identifier of source category',1,0,' ',' ',' ',1,1,1,0,1,0,1,0,' ','xesam:DataObject',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(256,'xesam:relevancyRating',5,NULL,'Query relevancy rating of the object ',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:DataObject',' ');
INSERT INTO "XesamMetaDataTypes" VALUES(257,'xesam:snippet',3,NULL,'Data snippet relevant to the search query ',1,0,' ',' ',' ',1,1,0,0,1,0,1,0,' ','xesam:DataObject',' ');
DELETE FROM sqlite_sequence;
INSERT INTO "sqlite_sequence" VALUES('XesamMetaDataTypes',257);
INSERT INTO "sqlite_sequence" VALUES('XesamServiceTypes',69);
INSERT INTO "sqlite_sequence" VALUES('XesamServiceMapping',12);
INSERT INTO "sqlite_sequence" VALUES('XesamMetaDataMapping',15);
INSERT INTO "sqlite_sequence" VALUES('XesamServiceLookup',57);
INSERT INTO "sqlite_sequence" VALUES('XesamMetaDataLookup',15);
CREATE TABLE XesamServiceTypes
(
	TypeID 			Integer primary key AUTOINCREMENT not null,
	TypeName		Text COLLATE NOCASE not null,

	TypeCount		Integer default 0,

	DisplayName		Text default ' ',
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
INSERT INTO "XesamServiceTypes" VALUES(1,'xesam:Annotation',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic annotation. Annotation provides a set of document description properties(like subject, title, description) for a list of objects it links to. It can link to other annotations, however interpretation of this may differ between specific annotation classes..',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(2,'xesam:Archive',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic archive',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(3,'xesam:ArchivedFile',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','File stored in an archive',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(4,'xesam:Audio',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Defines audio aspect of content. The content itself may have other aspects.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(5,'xesam:AudioList',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic audio list(playlist). Linking to other content types is forbidden',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(6,'xesam:BlockDevice',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic block device. Typically contains partitions/filesystems',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(7,'xesam:Bookmark',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default',' ',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(8,'xesam:CommunicationChannel',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Communication channel',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(9,'xesam:Contact',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Contact',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(10,'xesam:ContactGroup',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','ContactGroup',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(11,'xesam:Content',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic content',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(12,'xesam:DataObject',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic data object. Unites form and essense aspects represented by Content and Source. Used to aggreate properties that may be extracted from both content and source.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(13,'xesam:DeletedFile',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','File deleted to trash',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(14,'xesam:Document',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Document is an arrangement of various atomic data types with text being the primary data type.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(15,'xesam:Documentation',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Documentation is a document containing help, manuals, guides.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(16,'xesam:Email',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Email message',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(17,'xesam:EmailAttachment',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic storage',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(18,'xesam:EmbeddedObject',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic embedded/inlined object: attachment, inlined SVG, script etc.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(19,'xesam:File',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Regular file stored in a filesystem',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(20,'xesam:FileSystem',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Filesystem differs from other containers in that it has total/free/occupied space(though DBs too have similar properties), has volume(content.title), UUID for *ix(content.ID), mount point(if mounted)',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(21,'xesam:Filelike',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','File-like object',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(22,'xesam:Folder',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic folder. In general, folders represent a tree-like structure(taxonomy), however on occasion this rule may violated in cases like symlinks.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(23,'xesam:IMAP4Message',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','IMAP4 mailbox message',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(24,'xesam:IMMessage',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic Instant Messaging message',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(25,'xesam:Image',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Visual content',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(26,'xesam:MailingList',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Mailing list',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(27,'xesam:MailingListEmail',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Email message addressed at/received from a mailing list',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(28,'xesam:Media',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic raster/sampled media is considered consisting of Sets of Samples being reproduced(played/shown) at once. We describe: sample data type(int/float), data bit depth,configuration(color space for images, channel count for audio); set configuration(pixel dimensions for image); set count and rate.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(29,'xesam:MediaList',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic media content list(playlist). Linking to other content types is forbidden',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(30,'xesam:Message',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic message',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(31,'xesam:MessageboxMessage',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Message stored in a message box',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(32,'xesam:Music',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Music content',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(33,'xesam:NewsGroup',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','News group',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(34,'xesam:NewsGroupEmail',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Email message addressed at/received from a news group',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(35,'xesam:OfflineMedia',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic offline media. e.g. USB drive not attached at this moment.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(36,'xesam:Organization',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Organization',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(37,'xesam:POP3Message',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','POP3 mailbox message',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(38,'xesam:Person',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Person',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(39,'xesam:PersonalEmail',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Personal email message(not related to a mailing list or discussion group)',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(40,'xesam:Photo',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','An image with EXIF tags(photo)',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(41,'xesam:Presentation',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Presentation document',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(42,'xesam:Project',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic project',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(43,'xesam:RSSFeed',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','RSS feed',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(44,'xesam:RSSMessage',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','RSS message(RSS feed item)',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(45,'xesam:RemoteFile',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Remote file',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(46,'xesam:RemoteMessageboxMessage',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Remote messagebox message',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(47,'xesam:RemoteResource',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic remote resource',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(48,'xesam:RevisionControlledFile',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','File managed by a revision control system',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(49,'xesam:RevisionControlledRepository',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Revision-controlled repository. In case of distributed repositories, those must be linked with derivation relations.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(50,'xesam:SoftwarePackage',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Software distribution package',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(51,'xesam:Source',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic source',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(52,'xesam:SourceCode',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Source Code',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(53,'xesam:Spreadsheet',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Spreadsheet document',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(54,'xesam:SystemResource',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic system resource like man documentation',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(55,'xesam:Tag',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Tag/keyword. As opposed to folders, there are no limitations on the structure of tags and arbitrary overlaps are possible.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(56,'xesam:Text',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Defines a textual aspect of content. Properties represent only actual content intended for the user, not markup. Other parts of content like markup should be described using other clsses. Abstract class.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(57,'xesam:TextDocument',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Text document. For word processing apps.',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(58,'xesam:UncategorizedText',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default',' ',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(59,'xesam:Video',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Video content',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(60,'xesam:Visual',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Visual content',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(61,'xesam:XML',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','XML content',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(62,'xesam:Alarm',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Alarm',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(63,'xesam:ApplicationDesktopEntry',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Application Desktop Entry',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(64,'xesam:DesktopEntry',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Desktop Entry(typically a .desktop file)',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(65,'xesam:Event',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Event',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(66,'xesam:FreeBusy',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','FreeBusy',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(67,'xesam:Journal',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Journal',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(68,'xesam:PIM',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Generic PIM',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
INSERT INTO "XesamServiceTypes" VALUES(69,'xesam:Task',0,' ',1,1,0,0,1,1,0,0,1,1,1,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0,' ',' ',' ',' ','default','Task',0,' ',' ','stdout',' ',' ',' ',' ',' ',' ',' ');
CREATE TABLE XesamServiceMapping
(
	ID			Integer primary key AUTOINCREMENT not null,
	XesamTypeName		Text,
	TypeName		Text,

	unique (XesamTypeName, TypeName)
);
INSERT INTO "XesamServiceMapping" VALUES(1,'xesam:Audio','Audio');
INSERT INTO "XesamServiceMapping" VALUES(2,'xesam:Document','Documents');
INSERT INTO "XesamServiceMapping" VALUES(3,'xesam:Documentation','Documents');
INSERT INTO "XesamServiceMapping" VALUES(4,'xesam:Email','Email');
INSERT INTO "XesamServiceMapping" VALUES(5,'xesam:EmailAttachment','EmailAttachment');
INSERT INTO "XesamServiceMapping" VALUES(6,'xesam:File','File');
INSERT INTO "XesamServiceMapping" VALUES(7,'xesam:Folder','Folder');
INSERT INTO "XesamServiceMapping" VALUES(8,'xesam:Image','Image');
INSERT INTO "XesamServiceMapping" VALUES(9,'xesam:Music','Audio');
INSERT INTO "XesamServiceMapping" VALUES(10,'xesam:PersonalEmail','Email');
INSERT INTO "XesamServiceMapping" VALUES(11,'xesam:UncategorizedText','Text');
INSERT INTO "XesamServiceMapping" VALUES(12,'xesam:Video','Video');
CREATE TABLE XesamMetaDataMapping
(
	ID			Integer primary key AUTOINCREMENT not null,
	XesamMetaName		Text,
	MetaName		Text,

	unique (XesamMetaName, MetaName)
);
INSERT INTO "XesamMetaDataMapping" VALUES(1,'xesam:album','Audio:Album');
INSERT INTO "XesamMetaDataMapping" VALUES(2,'xesam:albumArtist','Audio:Artist');
INSERT INTO "XesamMetaDataMapping" VALUES(3,'xesam:artist','Audio:Artist');
INSERT INTO "XesamMetaDataMapping" VALUES(4,'xesam:author','Email:Sender');
INSERT INTO "XesamMetaDataMapping" VALUES(5,'xesam:cc','Email:CC');
INSERT INTO "XesamMetaDataMapping" VALUES(6,'xesam:genre','Audio:Genre');
INSERT INTO "XesamMetaDataMapping" VALUES(7,'xesam:primaryRecipient','Email:Recipient');
INSERT INTO "XesamMetaDataMapping" VALUES(8,'xesam:recipient','Email:Recipient');
INSERT INTO "XesamMetaDataMapping" VALUES(9,'xesam:secondaryRecipient','Email:Recipient');
INSERT INTO "XesamMetaDataMapping" VALUES(10,'xesam:title','Audio:Title');
INSERT INTO "XesamMetaDataMapping" VALUES(11,'xesam:title','Doc:Title');
INSERT INTO "XesamMetaDataMapping" VALUES(12,'xesam:title','Video:Title');
INSERT INTO "XesamMetaDataMapping" VALUES(13,'xesam:to','Email:Recipient');
INSERT INTO "XesamMetaDataMapping" VALUES(14,'xesam:trackNumber','Audio:TrackNo');
INSERT INTO "XesamMetaDataMapping" VALUES(15,'xesam:url','File:Name');
CREATE TABLE XesamServiceChildren
(
	Parent			Text,
	Child			Text,

	unique (Parent, Child)
);
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:Annotation');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:Archive');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Filelike','xesam:ArchivedFile');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Media','xesam:Audio');
INSERT INTO "XesamServiceChildren" VALUES('xesam:MediaList','xesam:AudioList');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Source','xesam:BlockDevice');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Annotation','xesam:Bookmark');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:CommunicationChannel');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:Contact');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:ContactGroup');
INSERT INTO "XesamServiceChildren" VALUES('xesam:DataObject','xesam:Content');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Filelike','xesam:DeletedFile');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Text','xesam:Document');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Document','xesam:Documentation');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Message','xesam:Email');
INSERT INTO "XesamServiceChildren" VALUES('xesam:EmbeddedObject','xesam:EmailAttachment');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Source','xesam:EmbeddedObject');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Filelike','xesam:File');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:FileSystem');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Source','xesam:Filelike');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Annotation','xesam:Folder');
INSERT INTO "XesamServiceChildren" VALUES('xesam:RemoteMessageboxMessage','xesam:IMAP4Message');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Message','xesam:IMMessage');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Visual','xesam:Image');
INSERT INTO "XesamServiceChildren" VALUES('xesam:ContactGroup','xesam:MailingList');
INSERT INTO "XesamServiceChildren" VALUES('xesam:CommunicationChannel','xesam:MailingList');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Email','xesam:MailingListEmail');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:Media');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Annotation','xesam:MediaList');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:Message');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Source','xesam:MessageboxMessage');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Audio','xesam:Music');
INSERT INTO "XesamServiceChildren" VALUES('xesam:CommunicationChannel','xesam:NewsGroup');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Email','xesam:NewsGroupEmail');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Source','xesam:OfflineMedia');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Contact','xesam:Organization');
INSERT INTO "XesamServiceChildren" VALUES('xesam:RemoteMessageboxMessage','xesam:POP3Message');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Contact','xesam:Person');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Email','xesam:PersonalEmail');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Image','xesam:Photo');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Document','xesam:Presentation');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Annotation','xesam:Project');
INSERT INTO "XesamServiceChildren" VALUES('xesam:CommunicationChannel','xesam:RSSFeed');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Message','xesam:RSSMessage');
INSERT INTO "XesamServiceChildren" VALUES('xesam:RemoteResource','xesam:RemoteFile');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Filelike','xesam:RemoteFile');
INSERT INTO "XesamServiceChildren" VALUES('xesam:RemoteResource','xesam:RemoteMessageboxMessage');
INSERT INTO "XesamServiceChildren" VALUES('xesam:MessageboxMessage','xesam:RemoteMessageboxMessage');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Source','xesam:RemoteResource');
INSERT INTO "XesamServiceChildren" VALUES('xesam:File','xesam:RevisionControlledFile');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:RevisionControlledRepository');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:SoftwarePackage');
INSERT INTO "XesamServiceChildren" VALUES('xesam:DataObject','xesam:Source');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Text','xesam:SourceCode');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Document','xesam:Spreadsheet');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Source','xesam:SystemResource');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Annotation','xesam:Tag');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:Text');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Document','xesam:TextDocument');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Text','xesam:UncategorizedText');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Audio','xesam:Video');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Visual','xesam:Video');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Media','xesam:Visual');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Text','xesam:XML');
INSERT INTO "XesamServiceChildren" VALUES('xesam:PIM','xesam:Alarm');
INSERT INTO "XesamServiceChildren" VALUES('xesam:DesktopEntry','xesam:ApplicationDesktopEntry');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:DesktopEntry');
INSERT INTO "XesamServiceChildren" VALUES('xesam:PIM','xesam:Event');
INSERT INTO "XesamServiceChildren" VALUES('xesam:PIM','xesam:FreeBusy');
INSERT INTO "XesamServiceChildren" VALUES('xesam:PIM','xesam:Journal');
INSERT INTO "XesamServiceChildren" VALUES('xesam:Content','xesam:PIM');
INSERT INTO "XesamServiceChildren" VALUES('xesam:PIM','xesam:Task');
CREATE TABLE XesamMetaDataChildren
(
	Parent			Text,
	Child			Text,

	unique (Parent, Child)
);
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:author','xesam:albumArtist');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:author','xesam:artist');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:mediaBitrate','xesam:audioBitrate');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:sampleConfiguration','xesam:audioChannels');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:mediaCodec','xesam:audioCodec');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:mediaCodecType','xesam:audioCodecType');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:sampleBitDepth','xesam:audioSampleBitDepth');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:setCount','xesam:audioSampleCount');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:sampleDataType','xesam:audioSampleDataType');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:setRate','xesam:audioSampleRate');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:creator','xesam:author');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:secondaryRecipient','xesam:bcc');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:contactURL','xesam:blogContactURL');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:secondaryRecipient','xesam:cc');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:phoneNumber','xesam:cellPhoneNumber');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:communicationChannel','xesam:chatRoom');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:sampleConfiguration','xesam:colorSpace');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:author','xesam:composer');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:related','xesam:conflicts');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:contactMedium','xesam:contactURL');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:depends','xesam:contains');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:comment','xesam:contentComment');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:keyword','xesam:contentKeyword');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:creator','xesam:contributor');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:legal','xesam:copyright');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:related','xesam:depends');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:links','xesam:derivedFrom');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:legal','xesam:disclaimer');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:contactMedium','xesam:emailAddress');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:phoneNumber','xesam:faxPhoneNumber');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:setCount','xesam:frameCount');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:setRate','xesam:frameRate');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:emailAddress','xesam:homeEmailAddress');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:phoneNumber','xesam:homePhoneNumber');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:physicalAddress','xesam:homePostalAddress');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:contactURL','xesam:homepageContactURL');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:contactMedium','xesam:imContactMedium');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:derivedFrom','xesam:inReplyTo');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:imContactMedium','xesam:ircContactMedium');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:isEncrypted','xesam:isSourceEncrypted');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:imContactMedium','xesam:jabberContactMedium');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:related','xesam:knows');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:legal','xesam:license');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:legal','xesam:licenseType');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:related','xesam:links');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:author','xesam:lyricist');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:communicationChannel','xesam:mailingList');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:physicalAddress','xesam:mailingPostalAddress');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:creator','xesam:maintainer');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:fingerprint','xesam:md5Hash');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:communicationChannel','xesam:newsGroup');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:author','xesam:performer');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:contactMedium','xesam:phoneNumber');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:contactMedium','xesam:physicalAddress');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:sampleBitDepth','xesam:pixelDataBitDepth');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:sampleDataType','xesam:pixelDataType');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:recipient','xesam:primaryRecipient');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:lineCount','xesam:rowCount');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:communicationChannel','xesam:rssFeed');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:recipient','xesam:secondaryRecipient');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:name','xesam:seenAttachedAs');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:fingerprint','xesam:sha1Hash');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:related','xesam:supercedes');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:primaryRecipient','xesam:to');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:comment','xesam:userComment');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:keyword','xesam:userKeyword');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:mediaBitrate','xesam:videoBitrate');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:mediaCodec','xesam:videoCodec');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:mediaCodecType','xesam:videoCodecType');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:emailAddress','xesam:workEmailAddress');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:phoneNumber','xesam:workPhoneNumber');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:physicalAddress','xesam:workPostalAddress');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:imContactMedium','xesam:aimContactMedium');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:imContactMedium','xesam:icqContactMedium');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:id','xesam:imdbId');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:id','xesam:isrc');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:imContactMedium','xesam:msnContactMedium');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:id','xesam:musicBrainzAlbumArtistID');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:id','xesam:musicBrainzAlbumID');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:id','xesam:musicBrainzArtistID');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:fingerprint','xesam:musicBrainzFingerprint');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:id','xesam:musicBrainzTrackID');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:imContactMedium','xesam:skypeContactMedium');
INSERT INTO "XesamMetaDataChildren" VALUES('xesam:imContactMedium','xesam:yahooContactMedium');
CREATE TABLE XesamServiceLookup
(	
	ID			Integer primary key AUTOINCREMENT not null,
	XesamTypeName		Text,
	TypeName		Text,

	unique (XesamTypeName, TypeName)
);
INSERT INTO "XesamServiceLookup" VALUES(1,'xesam:Annotation','Folder');
INSERT INTO "XesamServiceLookup" VALUES(3,'xesam:Audio','Audio');
INSERT INTO "XesamServiceLookup" VALUES(4,'xesam:Audio','Video');
INSERT INTO "XesamServiceLookup" VALUES(8,'xesam:Content','Video');
INSERT INTO "XesamServiceLookup" VALUES(10,'xesam:Content','Folder');
INSERT INTO "XesamServiceLookup" VALUES(11,'xesam:Content','Audio');
INSERT INTO "XesamServiceLookup" VALUES(12,'xesam:Content','Image');
INSERT INTO "XesamServiceLookup" VALUES(13,'xesam:Content','Email');
INSERT INTO "XesamServiceLookup" VALUES(14,'xesam:Content','Documents');
INSERT INTO "XesamServiceLookup" VALUES(15,'xesam:Content','Text');
INSERT INTO "XesamServiceLookup" VALUES(16,'xesam:DataObject','EmailAttachment');
INSERT INTO "XesamServiceLookup" VALUES(20,'xesam:DataObject','Video');
INSERT INTO "XesamServiceLookup" VALUES(22,'xesam:DataObject','Folder');
INSERT INTO "XesamServiceLookup" VALUES(23,'xesam:DataObject','Audio');
INSERT INTO "XesamServiceLookup" VALUES(24,'xesam:DataObject','Image');
INSERT INTO "XesamServiceLookup" VALUES(25,'xesam:DataObject','Email');
INSERT INTO "XesamServiceLookup" VALUES(26,'xesam:DataObject','Documents');
INSERT INTO "XesamServiceLookup" VALUES(27,'xesam:DataObject','Text');
INSERT INTO "XesamServiceLookup" VALUES(28,'xesam:DataObject','File');
INSERT INTO "XesamServiceLookup" VALUES(30,'xesam:Document','Documents');
INSERT INTO "XesamServiceLookup" VALUES(31,'xesam:Documentation','Documents');
INSERT INTO "XesamServiceLookup" VALUES(33,'xesam:Email','Email');
INSERT INTO "XesamServiceLookup" VALUES(34,'xesam:EmailAttachment','EmailAttachment');
INSERT INTO "XesamServiceLookup" VALUES(35,'xesam:EmbeddedObject','EmailAttachment');
INSERT INTO "XesamServiceLookup" VALUES(36,'xesam:File','File');
INSERT INTO "XesamServiceLookup" VALUES(37,'xesam:Filelike','File');
INSERT INTO "XesamServiceLookup" VALUES(38,'xesam:Folder','Folder');
INSERT INTO "XesamServiceLookup" VALUES(39,'xesam:Image','Image');
INSERT INTO "XesamServiceLookup" VALUES(40,'xesam:Media','Image');
INSERT INTO "XesamServiceLookup" VALUES(42,'xesam:Media','Audio');
INSERT INTO "XesamServiceLookup" VALUES(44,'xesam:Media','Video');
INSERT INTO "XesamServiceLookup" VALUES(46,'xesam:Message','Email');
INSERT INTO "XesamServiceLookup" VALUES(47,'xesam:Music','Audio');
INSERT INTO "XesamServiceLookup" VALUES(48,'xesam:PersonalEmail','Email');
INSERT INTO "XesamServiceLookup" VALUES(49,'xesam:Source','EmailAttachment');
INSERT INTO "XesamServiceLookup" VALUES(50,'xesam:Source','File');
INSERT INTO "XesamServiceLookup" VALUES(52,'xesam:Text','Documents');
INSERT INTO "XesamServiceLookup" VALUES(53,'xesam:Text','Text');
INSERT INTO "XesamServiceLookup" VALUES(54,'xesam:UncategorizedText','Text');
INSERT INTO "XesamServiceLookup" VALUES(55,'xesam:Video','Video');
INSERT INTO "XesamServiceLookup" VALUES(56,'xesam:Visual','Image');
INSERT INTO "XesamServiceLookup" VALUES(57,'xesam:Visual','Video');
CREATE TABLE XesamMetaDataLookup
(	
	ID			Integer primary key AUTOINCREMENT not null,
	XesamMetaName		Text,
	MetaName		Text,

	unique (XesamMetaName, MetaName)
);
INSERT INTO "XesamMetaDataLookup" VALUES(1,'xesam:album','Audio:Album');
INSERT INTO "XesamMetaDataLookup" VALUES(2,'xesam:albumArtist','Audio:Artist');
INSERT INTO "XesamMetaDataLookup" VALUES(3,'xesam:artist','Audio:Artist');
INSERT INTO "XesamMetaDataLookup" VALUES(4,'xesam:author','Email:Sender');
INSERT INTO "XesamMetaDataLookup" VALUES(5,'xesam:cc','Email:CC');
INSERT INTO "XesamMetaDataLookup" VALUES(6,'xesam:genre','Audio:Genre');
INSERT INTO "XesamMetaDataLookup" VALUES(7,'xesam:primaryRecipient','Email:Recipient');
INSERT INTO "XesamMetaDataLookup" VALUES(8,'xesam:recipient','Email:Recipient');
INSERT INTO "XesamMetaDataLookup" VALUES(9,'xesam:secondaryRecipient','Email:Recipient');
INSERT INTO "XesamMetaDataLookup" VALUES(10,'xesam:title','Audio:Title');
INSERT INTO "XesamMetaDataLookup" VALUES(11,'xesam:title','Doc:Title');
INSERT INTO "XesamMetaDataLookup" VALUES(12,'xesam:title','Video:Title');
INSERT INTO "XesamMetaDataLookup" VALUES(13,'xesam:to','Email:Recipient');
INSERT INTO "XesamMetaDataLookup" VALUES(14,'xesam:trackNumber','Audio:TrackNo');
INSERT INTO "XesamMetaDataLookup" VALUES(15,'xesam:url','File:Name');
COMMIT;

/* describes the types of metadata */
CREATE TABLE  MetaDataTypes 
(
	ID	 		Integer primary key AUTOINCREMENT not null,
	MetaName		Text  not null  COLLATE NOCASE, 
	DataTypeID		Integer  not null, /* 0=indexable string, 1= non-indexable string, 2=numeric, 3=datetime, 4=Blob, 5=keyword, 6=compressed fulltext */
	MultipleValues		Integer default 1, /* 0= type cannot have multiple values per entity, 1= type can have more than 1 value per entity */
	Weight			Integer  default 1 not null,  /* weight of metdata type in ranking */

	Unique (MetaName)
);


/* flattened table to store metadata inter-relationships */
CREATE TABLE  MetaDataChildren
(
	MetaDataID		integer  not null,
	ChildID			integer not null,

	primary key (MetaDataID, ChildID)

);


/* built in metadata types and their relationships*/

begin transaction;

/* Global generic Dublin Core types applicable to all metadata classes */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Title', 5, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Date', 3, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Modified', 3, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Created', 3, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Format', 0, 1, 20);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Creator', 0, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Rights', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:License', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Publisher', 0, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Identifier', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Source', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Relation', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Language', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Keywords', 5, 1, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Coverage', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Description', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Contributors',0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('DC:Type',0, 1, 10);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Date' and C.MetaName = 'DC:Created';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Date' and C.MetaName = 'DC:Modified';

/* File Specific metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Name', 0, 0, 20);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Ext', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:NameDelimited', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Path', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Contents', 6, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Link', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Mime', 5, 0, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Size', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:License', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Copyright', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Origin', 0, 0, 5);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:OriginURI', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Permissions', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Rank', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:IconPath', 1, 0, 0 );
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:SmallThumbnailPath', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:LargeThumbnailPath', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Modified', 3, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Accessed', 3, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('File:Other', 0, 0, 5);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Identifier' and C.MetaName = 'File:Name';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Identifier' and C.MetaName = 'File:Path';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Format' and C.MetaName = 'File:Ext';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Format' and C.MetaName = 'File:Mime';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Source' and C.MetaName = 'File:Origin';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Source' and C.MetaName = 'File:OriginURI';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:License' and C.MetaName = 'File:License';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:License' and C.MetaName = 'File:Copyright';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Modified' and C.MetaName = 'File:Modified';


/* Audio specific metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Title', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Artist', 0, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Album', 0, 1, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:AlbumArtist', 0, 1, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:AlbumTrackCount', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:TrackNo', 2, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:DiscNo', 2, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Performer', 0, 0, 30);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:TrackGain', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:TrackPeakGain', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:AlbumGain', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:AlbumPeakGain', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Duration', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:ReleaseDate', 3, 1, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Comment', 0, 0, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Genre', 5, 1, 75);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Codec', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:CodecVersion', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Samplerate', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Bitrate', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Channels', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:LastPlay', 3, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:PlayCount', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:IsNew', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:MBAlbumID', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:MBArtistID', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:MBAlbumArtistID', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:MBTrackID', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:Lyrics', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Audio:CoverAlbumThumbnailPath', 1, 0, 0);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Audio:Title';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Audio:Artist';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Audio:AlbumArtist';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Created' and C.MetaName = 'Audio:ReleaseDate';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Description' and C.MetaName = 'Audio:Comment';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Type' and C.MetaName = 'Audio:Genre';


/* Document specific metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Title', 0, 0, 60);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Subject', 0, 0, 70);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Author', 0, 0, 60);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Keywords', 5, 1, 80);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Comments', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:PageCount', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:WordCount', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Doc:Created', 3, 0, 0);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Doc:Title';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Doc:Subject';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Doc:Author';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Keywords' and C.MetaName = 'Doc:Keywords';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Descritpion' and C.MetaName = 'Doc:Comments';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Created' and C.MetaName = 'Doc:Created';

/* Image specific metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Height', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Width', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Title', 0, 0, 60);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Album', 0, 0, 30);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Date', 3,  0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Keywords', 5, 0, 100);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Creator', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Comments', 0, 0, 20);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Description', 0, 0, 15);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Software', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:CameraMake', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:CameraModel', 0, 0, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Orientation', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:ExposureProgram', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:ExposureTime', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:FNumber', 2 , 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:Flash', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:FocalLength', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:ISOSpeed', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:MeteringMode', 1, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Image:WhiteBalance', 1, 0, 0);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Image:Title';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Created' and C.MetaName = 'Image:Date';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Image:Creator';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Description' and C.MetaName = 'Image:Comments';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Description' and C.MetaName = 'Image:Description';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Keywords' and C.MetaName = 'Image:Keywords';

/* video metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Title', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Author', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Comments', 0, 0, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Height', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Width', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:FrameRate', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Codec', 0, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Bitrate', 2, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Video:Duration', 2, 0, 0);

insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Video:Title';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Video:Author';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Description' and C.MetaName = 'Video:Comments';

/* email metadata */
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Body', 6, 0, 1);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Date', 3, 0, 0);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Sender', 0, 0, 35);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:SentTo', 0, 1, 25);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Attachments', 0, 1, 20);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:AttachmentsDelimted', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:CC', 0, 1, 10);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Subject', 0, 0, 50);
insert Into MetaDataTypes (MetaName, DatatypeID, MultipleValues, Weight) values  ('Email:Recipient', 0, 1, 1);


insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Created' and C.MetaName = 'Email:Date';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Creator' and C.MetaName = 'Email:Sender';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'DC:Title' and C.MetaName = 'Email:Subject';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'Email:Recipient' and C.MetaName = 'Email:SentTo';
insert Into MetaDataChildren (MetaDataID, ChildID) select P.ID, C.ID from MetaDataTypes P, MetaDataTypes C where P.MetaName = 'Email:Recipient' and C.MetaName = 'Email:CC';

end transaction;

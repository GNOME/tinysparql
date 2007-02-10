CREATE TABLE  ServiceTypes
(
	TypeID 			Integer Primary Key not null,
	MinID			Integer,
	MaxID			Integer,
	TypeClass		Text COLLATE NOCASE,
	TypeName		Text COLLATE NOCASE,
	Description		Text COLLATE NOCASE,
	Database		int default 0, /* 0 = DB_DATA, 1 = DB_BLOB, 2 = DB_EMAIL, 3 = DB_CACHE */
	Icon			Text,
	ThumbExec		Text,
	MetadataExec		Text,
	FullTextExec		Text,
	ViewExec		Text,
	MainService	  	Integer  default 0

);

insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (0,0,8, 'Files',  'Files', 'Files', 1);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (1,1,1, 'Files', 'Folders', 'Folders', 0 );
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (2,2,2, 'Files', 'Documents', 'Documents', 1);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (3,3,3, 'Files', 'Images', 'Images', 1);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (4,4,4, 'Files', 'Music', 'Music', 1);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (5,5,5, 'Files', 'Videos', 'Videos', 1);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (6,6,6, 'Files', 'Text', 'Text Files', 1);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (7,7,7, 'Files', 'Development', 'Development Files', 1);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (8,8,8, 'Files', 'Other', 'Other Files', 0);

insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (9,9,17,   'VFS', 'VFS', 'VFS Files', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (10,10,10, 'VFS', 'VFSFolders', 'VFS Folders', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (11,11,11, 'VFS', 'VFSDocuments', 'VFS Documents', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (12,12,12, 'VFS', 'VFSImages', 'VFS Images', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (13,13,13, 'VFS', 'VFSMusic', 'VFS Music', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (14,14,14, 'VFS', 'VFSVideos', 'VFS Videos', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (15,15,15, 'VFS', 'VFSText', 'VFS Text Files', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (16,16,16, 'VFS', 'VFSDevelopment', 'VFS Development Files', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (17,17,17, 'VFS', 'VFSOther', 'VFS Other Files', 0);


insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (20,20,29,  'Emails',  'Emails', 'Emails', 1);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (21,21,21, 'Emails','EvolutionEmails', 'Evolution Emails', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (22,22,22, 'Emails','ThunderbirdEmails', 'Thunderbird Emails', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (23,23,23, 'Emails','KMailEmails', 'KMail Emails', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (29,29,29, 'Emails','OtherEmails', 'Other Emails', 0);

insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (30,30,39,  'EmailAttachments', 'EmailAttachments' ,'Email Attachments', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (31,31,31, 'EmailAttachments', 'EvolutionAttachments', 'Evolution Email Attachments', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (32,32,32, 'EmailAttachments', 'ThunderbirdAttachments', 'Thunderbird Email Attachments', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (33,33,33, 'EmailAttachments', 'KMailAttachments', 'KMail Email Attachments', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (39,39,39, 'EmailAttachments', 'OtherAttachments', 'Other Email Attachments', 0);

update ServiceTypes set Database = 2 where TypeClass in ('Emails', 'EmailAttachments');


insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (40,40,49, 'Conversations', 'Conversations', 'Conversations', 1);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (41,41,41, 'Conversations', 'GaimConversations', 'Gaim Conversations', 0);
insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (42,42,42, 'Conversations', 'XChatConversations', 'XChat Conversations', 0);

insert Into ServiceTypes (TypeID, MinID, MaxID, TypeClass, TypeName, Description, MainService) values (50,50,50, 'Applications', 'Applications', 'Applications', 1);

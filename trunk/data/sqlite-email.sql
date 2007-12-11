
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

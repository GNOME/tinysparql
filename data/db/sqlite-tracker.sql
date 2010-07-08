
CREATE TABLE Options (
	OptionKey 	Text COLLATE NOCASE PRIMARY KEY not null,
	OptionValue	Text COLLATE NOCASE
);

insert Into Options (OptionKey, OptionValue) values ('KMailLastModseq', '0');


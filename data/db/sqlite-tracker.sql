
CREATE TABLE Options (	
	OptionKey 	Text COLLATE NOCASE PRIMARY KEY not null,
	OptionValue	Text COLLATE NOCASE
);

insert Into Options (OptionKey, OptionValue) values ('DBVersion', '20');
insert Into Options (OptionKey, OptionValue) values ('Sequence', '1');
insert Into Options (OptionKey, OptionValue) values ('EventSequence', '1');
insert Into Options (OptionKey, OptionValue) values ('UpdateCount', '0');
insert Into Options (OptionKey, OptionValue) values ('EvolutionLastModseq', '0');
insert Into Options (OptionKey, OptionValue) values ('KMailLastModseq', '0');
insert Into Options (OptionKey, OptionValue) values ('RssLastModseq', '0');
insert Into Options (OptionKey, OptionValue) values ('CollationLocale', '');



/* basic info for a user defined file or service object */
CREATE TABLE  UserServices
(
	ID            		Integer primary key not null,
	Path 			Text  not null, 
	Name	 		Text, 

    	unique (Path, Name)

);



/* de-normalised metadata which is unprocessed (not normalized or casefolded) for display only - never used for searching */
CREATE TABLE  UserMetaDataDisplay 
(
	ServiceID		Integer not null,
	MetaDataID 		Integer  not null,
	UserValue		Text,
	Length			Integer.	
	
	primary key (ServiceID, MetaDataID)
);


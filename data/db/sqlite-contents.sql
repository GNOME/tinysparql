
CREATE TABLE ServiceContents 
(
	ServiceID Int not null, 
	MetadataID Int not null, 
	Content Text, 
	primary key (ServiceID, MetadataID)
);
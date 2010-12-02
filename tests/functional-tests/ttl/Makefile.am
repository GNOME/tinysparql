include $(top_srcdir)/Makefile.decl

BUILT_SOURCES = gen-test-data.stamp

gen-test-data.stamp:
	@echo "Generate data"
	mkdir ttl
	${top_srcdir}/utils/data-generators/cc/generate ${top_srcdir}/utils/data-generators/cc/max.cfg
	mv ttl/* . && rmdir ttl
	touch gen-test-data.stamp

ttldir = $(datadir)/tracker-tests/ttl

ttl_DATA =                                             \
	010-nco_EmailAddress.ttl                       \
	011-nco_PostalAddress.ttl                      \
	012-nco_PhoneNumber.ttl                        \
	013-nco_IMAddress.ttl                          \
	014-nco_ContactEmail.ttl                       \
	015-nco_ContactCall.ttl                        \
	016-nco_ContactIM.ttl                          \
	018-nco_PersonContact.ttl                      \
	020-mlo_GeoPoint.ttl                           \
	021-mlo_LocationBoundingBox.ttl                \
	028-mlo_GeoLocation.ttl                        \
	029-mlo_Landmark.ttl                           \
	030-nmo_MailAccount.ttl                        \
	031-nmo_MailFolder.ttl                         \
	032-nmo_Email.ttl                              \
	035-nmo_CommunicationChannel.ttl               \
	036-nmo_IMMessage.ttl                          \
	037-nmo_SMSMessage.ttl                         \
	038-nmo_Call.ttl                               \
	040-nmm_Artist.ttl                             \
	040-nmm_MusicAlbumDisc.ttl                     \
	041-nmm_MusicAlbum.ttl                         \
	042-nmm_MusicPiece.ttl                         \
	045-nmm_Photo.ttl                              \
	044-nfo_Equipment.ttl                          \
	046-nmm_Video.ttl                              \
	050-tracker_Volume.ttl                         \
	051-nfo_PlainTextDocument.ttl                  \
	060-nfo_SoftwareCategory.ttl                   \
	061-nfo_SoftwareApplication.ttl                \
	065-nfo_WebHistory.ttl                         \
	070-ncal_Alarm.ttl                             \
	071-ncal_Calendar.ttl                          \
	072-ncal_Event.ttl                             \
	073-ncal_Todo.ttl                              \
	080-mfo_FeedChannel.ttl                        \
	081-mfo_FeedMessage.ttl                        \
	090-mto_TransferElement.ttl                    \
	091-mto_UploadTransfer.ttl

DISTCLEANFILES =                                       \
	$(BUILT_SOURCES)                               \
	$(ttl_DATA)                                    \
	source-data.pkl


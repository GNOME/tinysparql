barnum_dir = join_paths(meson.current_source_dir(), '..', '..', '..', 'utils', 'data-generators', 'cc')

barnum = join_paths(barnum_dir, 'generate')
barnum_config = join_paths(barnum_dir, 'max.cfg')

barnum_outputs = [
  '010-nco_EmailAddress.ttl',
  '011-nco_PostalAddress.ttl',
  '012-nco_PhoneNumber.ttl',
  '013-nco_IMAddress.ttl',
  '014-nco_ContactEmail.ttl',
  '015-nco_ContactCall.ttl',
  '016-nco_ContactIM.ttl',
  '018-nco_PersonContact.ttl',
  '020-slo_GeoLocation.ttl',
  '021-slo_Landmark.ttl',
  '026-mlo_GeoPoint.ttl',
  '027-mlo_LocationBoundingBox.ttl',
  '028-mlo_GeoLocation.ttl',
  '029-mlo_Landmark.ttl',
  '030-nmo_MailAccount.ttl',
  '031-nmo_MailFolder.ttl',
  '032-nmo_Email.ttl',
  '035-nmo_CommunicationChannel.ttl',
  '036-nmo_IMMessage.ttl',
  '037-nmo_SMSMessage.ttl',
  '038-nmo_Call.ttl',
  '040-nmm_Artist.ttl',
  '040-nmm_MusicAlbumDisc.ttl',
  '041-nmm_MusicAlbum.ttl',
  '042-nmm_MusicPiece.ttl',
  '045-nmm_Photo.ttl',
  '044-nfo_Equipment.ttl',
  '046-nmm_Video.ttl',
  '050-tracker_Volume.ttl',
  '051-nfo_PlainTextDocument.ttl',
  '060-nfo_SoftwareCategory.ttl',
  '061-nfo_SoftwareApplication.ttl',
  '065-nfo_WebHistory.ttl',
  '070-ncal_Alarm.ttl',
  '071-ncal_Calendar.ttl',
  '072-ncal_Event.ttl',
  '073-ncal_Todo.ttl',
  '080-mfo_FeedChannel.ttl',
  '081-mfo_FeedMessage.ttl',
  '090-mto_TransferElement.ttl',
  '091-mto_UploadTransfer.ttl',
]


barnum_test_data = custom_target('barnum-test-data',
  command: [barnum, barnum_config, meson.current_build_dir()],
  output: barnum_outputs,
  build_by_default: true)

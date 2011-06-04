if (!org.bustany.TrackerBird.TrackerStore || !org.bustany.TrackerBird.TrackerStore.__initialized)
org.bustany.TrackerBird.TrackerStore = {
	// Init barrier
	__initialized: true,

	_addressParser: org.bustany.TrackerBird.EmailAddressParser,
	_tracker: org.bustany.TrackerBird.TrackerSparql,

	storeMessage: function(folder, header) {
		var uri = folder.getUriForMsg(header);
		var fromEmailAddress;
		var toEmailAddresses = [];

		fromEmailAddress = this._addressParser.parse(header.mime2DecodedAuthor);

		// FIXME: this is not bullet-proof, but will do for now
		var unparsedRecipients = header.mime2DecodedRecipients.split(">,");
		for (var i in unparsedRecipients) {
			var parsedAddress = this._addressParser.parse(unparsedRecipients[i] + ">");

			if (parsedAddress) {
				toEmailAddresses.push(parsedAddress);
			}
		}

		var query = this.baseQuery(uri, header)
		          + this.contactQuery("nmo:from", fromEmailAddress)
		          + this.contactsQuery("nmo:to", toEmailAddresses);

		if ((header.flags & Components.interfaces.nsMsgMessageFlags.Offline)
		 || (header.folder instanceof Components.interfaces.nsIMsgLocalMailFolder)) {
			 // We can access mail body
		}

		dump(query+"\n");
	},

	baseQuery: function(uri, hdr) {
		var query = "<" + uri + "> a nmo:Email, nmo:MailboxDataObject";
		          + "; nie:url \"" + uri + "\" ";

		if (hdr.mime2DecodedSubject != "") {
			query += "; nmo:messageSubject \""
			       + this.escapeString(hdr.mime2DecodedSubject)
				   + "\" ";
		}

		query += "; nmo:sentDate \""
		       + new Date(hdr.dateInSeconds).toISOString()
			   + "\" ";

		return query;
	},

	contactQuery: function(predicate, address) {
		if (address == null) {
			return "";
		}

		var nameSparql = "";

		if (address.name != "") {
			nameSparql = "; nco:fullname: \""
			           + this.escapeString(address.name)
			           + "\" ";
		}

		var emailSparql = "; nco:hasEmailAddress <mailto:"
		                + address.address
		                + "> ";

		return "; " + predicate + " [ a nco:Contact " + nameSparql + emailSparql + "] ";
	},

	contactsQuery: function(predicate, addresses) {
		var query = "";

		for (var i in addresses) {
			query += this.contactQuery(predicate, addresses[i]);
		}

		return query;
	},

	escapeString: function(str) {
		return this._tracker.escape_string(str).readString();
	}
}

if (!org.bustany.TrackerBird.EmailAddressParser || !org.bustany.TrackerBird.EmailAddressParser.__initialized)
org.bustany.TrackerBird.EmailAddressParser = {
	// Init barrier
	__initialized: true,

	// Parses an email address in a {name: address:} object
	parse: function(str) {
		var longAddr = /\s*(.*?)\s*<([^\x00-\x20\(\)<>@,;:\\"\[\]]+@[^\x00-\x20\(\)<>@,;:\\"\[\]]+)>\s*$/;
		var addrSpec = /\s*([^\x00-\x20\(\)<>@,;:\\"\[\]]+@[^\x00-\x20\(\)<>@,;:\\"\[\]]+)\s*/;

		var matches = str.match(longAddr);

		if (matches != null) {
			return {
			        name: matches[1].replace(/"(.+)"/, "$1"),
			        address: matches[2]
			       };
		}

		matches = str.match(addrSpec);

		if (matches != null) {
			return {
			        name: "",
			        address: matches[1]
			       };
		}

		dump("Could not parse email address " + str + "\n");

		return null;
	}
}

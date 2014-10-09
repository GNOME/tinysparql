<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output method="xml" omit-xml-declaration="yes" disable-output-escaping="yes"/>

<xsl:template match="/">
.TH <xsl:value-of select="$config-file"/> 5 "<xsl:value-of select="$current-date"/>" <xsl:value-of select="$version"/> "Tracker Manual"

.SH NAME
<xsl:value-of select="$config-file"/>

.SH SYNOPSIS
$HOME/.config/tracker/<xsl:value-of select="$config-file"/>

.SH DESCRIPTION
Tracker's configuration is built on top of GSettings, part of GLib. This means that there is a proper schema for configurations and they can be viewed (normally) using the \fBdconf-editor\fR tool.

Tracker also allows switching from the GSettings database, used by most (cross) desktop applications, to a key/value formatted files (like Microsoft's INI format). To do this, the environment variable \fBTRACKER_USE_CONFIG_FILES\fR must be defined before running the application using that configuration.

So where is this configuration? Well, normally they're stored in \fI$HOME/.config/tracker/\fR, however, default values are not stored to config files, only \fBdifferent\fR values are. This man page describes what keys and values can be used.

See EXAMPLES for a general overview.

.SH OPTIONS
<xsl:for-each select="schemalist/schema/key">
.TP
\fB<xsl:value-of select="@name"/>\fR=<xsl:value-of select="translate(default, '(\[ | \])', '')" disable-output-escaping="yes"/>
.nf

<xsl:value-of select="description"  disable-output-escaping="yes"/>

<xsl:for-each select="range">
Values range from <xsl:value-of select="@min"/> to <xsl:value-of select="@max"/>.
</xsl:for-each>
.fi

</xsl:for-each>

.SH EXAMPLES
The top level group is "General". The default configuration (if saved to <xsl:value-of select="$config-file"/>), would look like:

.nf
    [General]
    <xsl:for-each select="schemalist/schema/key">
      <xsl:value-of select="@name"/>=<xsl:value-of select="translate(default, '(\[ | \])', '')" disable-output-escaping="yes"/>;
    </xsl:for-each>
.fi

.SH SEE ALSO
.BR <xsl:value-of select="translate($config-file, '.', '\n')"/>

</xsl:template>

</xsl:stylesheet>

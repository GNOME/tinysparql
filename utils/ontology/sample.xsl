<xsl:stylesheet version = '1.0'
     xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>
<xsl:output method="text" />
<xsl:template match="service">
<xsl:for-each select="section">
// item#<xsl:value-of select="position()"/> for <xsl:value-of select="@fullname" />
get<xsl:value-of select="@name"/>Of<xsl:value-of select="@prefix" /> {
    <xsl:for-each select="item">
  <xsl:value-of select="@name"/> {
       full   : <xsl:value-of select="@fullvalue"/>
       prefix : <xsl:value-of select="prefix" />
       value  : <xsl:value-of select="value" />
    }
  </xsl:for-each>
}
</xsl:for-each>
</xsl:template>
</xsl:stylesheet>

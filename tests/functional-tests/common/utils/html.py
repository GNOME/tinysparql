#!/usr/bin/env python
import unittest
import os

class html:
	
	def top(self):

	        os.remove('indexing-performance')	
		self.file = 'indexing-performance' 
		self.f = open(self.file, "a")
		self.f.write('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">' + "\n" +
		'<html>' + "\n" +
		' <head>' + "\n" +
		' <meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1">' + "\n" +
		' <title>Tracker Indexing Performance</title>' + "\n" +
		' <H1 align="center"><font color="#CC0000" face="verdana" size="6">Tracker Indexing Performance</font></H1>' + "\n" +
		' <body>' + "\n" +
		' <table border="1", align="center">' + "\n" + 
		'<th><font color="#8000FF" face="verdana" size="4">Test data</font></th>' + "\n" +
		'<th><font color="#8000FF" face="verdana" size="4">Minimum</font></th>' + "\n" +
		'<th><font color="#8000FF" face="verdana" size="4">Maximum</font></th>' + "\n" +
		'<th><font color="#8000FF" face="verdana" size="4">Average</font></th>' + "\n" +
		'<th><font color="#8000FF" face="verdana" size="4">Median</font></th>' + "\n" 
		)
		self.f.close() 
		
	
	def mid(self,title,min,max,avg,median):

		self.file = 'indexing-performance' 
		self.f = open(self.file, "a")
		self.f.write( '<tr>' + "\n" +
		'<td>' + title + '</td>' + "\n" +
		'<td>' + str(min) + '</td>' + "\n" +
		'<td>' + str(max) + '</td>' + "\n" +
		'<td>' + str(avg) + '</td>' + "\n" +
		'<td>' + str(median) + '</td>' + "\n" +
		'</tr>' + "\n" 
		)
		self.f.close() 

	def bottom(self):

		self.file = 'indexing-performance' 
		self.f = open(self.file, "a")
		self.f.write( '</table>' + "\n" +
		' </body>' + "\n" +
		' </head>' + "\n" +
		' </html>' + "\n" 
		)
		self.f.close() 

class report(unittest.TestCase):

        def first(self):                      
                self.file = html()
                self.file.top()                                    
                                                             
        def last(self):                                        
                self.file = html()
                self.file.bottom()



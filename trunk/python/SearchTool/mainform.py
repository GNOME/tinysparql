#!/usr/bin/env python
#    Created by Eugenio Cutolo  <me@eugesoftware.com> using QtDesinger
#
#    This program can be distributed under the terms of the GNU GPL.
#    See the file COPYING.
#

#QtTracker Relase 0.1

from qt import *

class QRichListViewItem(QListViewItem):
	def __init__(self,*args):
		QListViewItem.__init__(self,*args)
		self.richtext = 0
		self.indent = 0
		self.rText = QString()
		self.recreateRichText();

	def setText(self,column,text):
		if column == 1:
			self.rText = text
			self.recreateRichText();
		else:
			QListViewItem.setText(self,column,text)

	def recreateRichText(self):
		if  self.richtext != 0:
			del(self.richtext)
			self.richtext = 0
		self.richtext = QSimpleRichText("<small>"+self.rText+"</small>", self.listView().font())

	def widthChanged(self,c):
		if c == -1 or c == 1:
			self.richtext.setWidth(self.listView().columnWidth(1)-15)
		QListViewItem.widthChanged(self,c)

	def paintCell(self, p, cg, column, width, align):
		if column == 1:
			paper = QBrush()
		
			palette = self.listView().viewport().palette()
			itemRectangle = self.listView().itemRect(self)
			itemRectangle.setX(self.listView().columnWidth(0))
			self.listView().viewport().erase(itemRectangle)
			colourGroup = QColorGroup(cg)
			
			if  self.isSelected() == 1:
				paper = QBrush(cg.highlight())
			else:
				txtcolor = QColor(100,100,100)
				colourGroup.setColor(QColorGroup.Text,txtcolor)
			width = self.listView().width() - self.listView().columnWidth(0)
			self.listView().setColumnWidth(1,width)
			self.richtext.draw( p,self.listView().itemMargin(), 0, QRect( 0, 0, width, self.height() ), colourGroup, paper );
			self.widthChanged(1)
		else:
			QListViewItem.paintCell(self, p, cg, column,width, align)
				
class MainForm(QDialog):
	def __init__(self,parent = None,name = None,modal = 0,fl = 0):
		QDialog.__init__(self,parent,name,modal,fl)

		if not name:
			self.setName("MainForm")

		self.setSizePolicy(QSizePolicy(QSizePolicy.Expanding,QSizePolicy.Expanding,0,0,self.sizePolicy().hasHeightForWidth()))
		self.setModal(1)

		MainFormLayout = QGridLayout(self,1,1,11,6,"MainFormLayout")

		self.mainframe = QFrame(self,"mainframe")
		self.mainframe.setSizePolicy(QSizePolicy(QSizePolicy.Expanding,QSizePolicy.Expanding,0,0,self.mainframe.sizePolicy().hasHeightForWidth()))
		self.mainframe.setFrameShape(QFrame.StyledPanel)
		self.mainframe.setFrameShadow(QFrame.Raised)
		mainframeLayout = QGridLayout(self.mainframe,1,1,11,6,"mainframeLayout")

		self.nextbtn = QPushButton(self.mainframe,"nextbtn")
		self.nextbtn.setEnabled(0)

		mainframeLayout.addWidget(self.nextbtn,2,5)

		self.prevbtn = QPushButton(self.mainframe,"prevbtn")
		self.prevbtn.setEnabled(0)

		mainframeLayout.addWidget(self.prevbtn,2,4)

		self.mode_combo = QComboBox(0,self.mainframe,"mode_combo")

		mainframeLayout.addWidget(self.mode_combo,0,2)

		self.searchinp = QLineEdit(self.mainframe,"searchinp")

		mainframeLayout.addWidget(self.searchinp,0,1)

		self.pagen_display = QLabel(self.mainframe,"pagen_display")

		mainframeLayout.addMultiCellWidget(self.pagen_display,2,2,0,3)

		self.services_combo = QComboBox(0,self.mainframe,"services_combo")

		mainframeLayout.addMultiCellWidget(self.services_combo,0,0,3,4)

		self.findbtn = QPushButton(self.mainframe,"findbtn")

		mainframeLayout.addWidget(self.findbtn,0,5)

		self.textLabel1 = QLabel(self.mainframe,"textLabel1")

		mainframeLayout.addWidget(self.textLabel1,0,0)

		self.result_list = QListView(self.mainframe,"result_list")
		self.result_list.addColumn(self.__tr("Info"))
		self.result_list.header().setResizeEnabled(0,self.result_list.header().count() - 1)
		self.result_list.addColumn(self.__tr("Text"))
		self.result_list.setSizePolicy(QSizePolicy(QSizePolicy.Expanding,QSizePolicy.Expanding,0,0,self.result_list.sizePolicy().hasHeightForWidth()))
		self.result_list.setLineWidth(1)
		self.result_list.setResizeMode(QListView.LastColumn)
		self.result_list.setMargin(0)
		self.result_list.setMidLineWidth(0)
		self.result_list.setResizePolicy(QScrollView.Manual)
		self.result_list.setHScrollBarMode(QListView.AlwaysOff)
		self.result_list.header().hide()
		self.result_list.setAllColumnsShowFocus(0)
		self.result_list.setShowSortIndicator(0)
		self.result_list.setItemMargin(5)
		self.result_list.setRootIsDecorated(0)

		mainframeLayout.addMultiCellWidget(self.result_list,1,1,0,5)

		MainFormLayout.addWidget(self.mainframe,0,0)

		self.languageChange()

		self.resize(QSize(629,520).expandedTo(self.minimumSizeHint()))
		self.clearWState(Qt.WState_Polished)

		self.connect(self.searchinp,SIGNAL("returnPressed()"),self.searchinp_returnPressed)
		self.connect(self.services_combo,SIGNAL("activated(int)"),self.services_combo_textChanged)
		self.connect(self.findbtn,SIGNAL("clicked()"),self.findbtn_clicked)
		self.connect(self.nextbtn,SIGNAL("clicked()"),self.nextbtn_clicked)
		self.connect(self.prevbtn,SIGNAL("clicked()"),self.prevbtn_clicked)
		self.connect(self.result_list,SIGNAL("doubleClicked(QListViewItem*)"),self.result_list_doubleClicked)
		self.connect(self.result_list,SIGNAL("contextMenuRequested(QListViewItem*,const QPoint&,int)"),self.result_list_contextMenuRequested)


	def languageChange(self):
		self.setCaption(self.__tr("Search Tool"))
		self.nextbtn.setText(self.__tr("Next"))
		self.prevbtn.setText(self.__tr("Previous"))
		self.mode_combo.clear()
		self.mode_combo.insertItem(self.__tr("key"))
		self.mode_combo.insertItem(self.__tr("tag"))
		self.pagen_display.setText(QString.null)
		self.services_combo.clear()
		self.services_combo.insertItem(self.__tr("All Files"))
		self.services_combo.insertItem(self.__tr("Development"))
		self.services_combo.insertItem(self.__tr("Documents"))
		self.services_combo.insertItem(self.__tr("Images"))
		self.services_combo.insertItem(self.__tr("Music"))
		self.services_combo.insertItem(self.__tr("Plain Text"))
		self.services_combo.insertItem(self.__tr("Videos"))
		self.findbtn.setText(self.__tr("Find"))
		self.textLabel1.setText(self.__tr("Search:"))
		self.result_list.header().setLabel(0,self.__tr("Info"))
		self.result_list.header().setLabel(1,self.__tr("Text"))
		QToolTip.add(self.result_list,self.__tr("Lista Risultati"))

	def services_combo_textChanged(self,int):
		self.findbtn_clicked()

	def findbtn_clicked(self):
		print "MainForm.findbtn_clicked(): Not implemented yet"

	def prevbtn_clicked(self):
		print "MainForm.prevbtn_clicked(): Not implemented yet"

	def nextbtn_clicked(self):
		print "MainForm.nextbtn_clicked(): Not implemented yet"

	def result_list_doubleClicked(self,a0):
		print "MainForm.result_list_doubleClicked(QListViewItem*): Not implemented yet"

	def result_list_contextMenuRequested(self,a0,a1,a2):
		print "MainForm.result_list_contextMenuRequested(QListViewItem*,const QPoint&,int): Not implemented yet"

	def searchinp_returnPressed(self):
		print "MainForm.searchinp_returnPressed(): Not implemented yet"

	def __tr(self,s,c = None):
		return qApp.translate("MainForm",s,c)

# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'qtCtrlWindow.ui'
#
# Created by: PyQt5 UI code generator 5.6
#
# WARNING! All changes made in this file will be lost!

from PyQt5 import QtCore, QtGui, QtWidgets

class Ui_CtrlWindow(object):
    def setupUi(self, CtrlWindow):
        CtrlWindow.setObjectName("CtrlWindow")
        CtrlWindow.resize(534, 26)
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(CtrlWindow.sizePolicy().hasHeightForWidth())
        CtrlWindow.setSizePolicy(sizePolicy)
        font = QtGui.QFont()
        font.setFamily("等线")
        CtrlWindow.setFont(font)
        self.centralwidget = QtWidgets.QWidget(CtrlWindow)
        self.centralwidget.setObjectName("centralwidget")
        self.cmdList = QtWidgets.QComboBox(self.centralwidget)
        self.cmdList.setGeometry(QtCore.QRect(2, 2, 87, 22))
        self.cmdList.setObjectName("cmdList")
        self.cmdList.addItem("")
        self.cmdList.addItem("")
        self.cmdList.addItem("")
        self.cmdList.addItem("")
        self.cmdList.addItem("")
        self.cmdList.addItem("")
        self.param1 = QtWidgets.QLineEdit(self.centralwidget)
        self.param1.setGeometry(QtCore.QRect(92, 2, 51, 22))
        self.param1.setReadOnly(True)
        self.param1.setObjectName("param1")
        self.param2 = QtWidgets.QLineEdit(self.centralwidget)
        self.param2.setGeometry(QtCore.QRect(146, 2, 51, 22))
        self.param2.setReadOnly(True)
        self.param2.setObjectName("param2")
        self.command = QtWidgets.QLineEdit(self.centralwidget)
        self.command.setGeometry(QtCore.QRect(200, 2, 281, 22))
        self.command.setReadOnly(True)
        self.command.setObjectName("command")
        self.sendButton = QtWidgets.QPushButton(self.centralwidget)
        self.sendButton.setGeometry(QtCore.QRect(483, 1, 50, 24))
        self.sendButton.setObjectName("sendButton")
        CtrlWindow.setCentralWidget(self.centralwidget)

        self.retranslateUi(CtrlWindow)
        QtCore.QMetaObject.connectSlotsByName(CtrlWindow)

    def retranslateUi(self, CtrlWindow):
        _translate = QtCore.QCoreApplication.translate
        CtrlWindow.setWindowTitle(_translate("CtrlWindow", "控制台"))
        self.cmdList.setItemText(0, _translate("CtrlWindow", "命令"))
        self.cmdList.setItemText(1, _translate("CtrlWindow", "设置边界"))
        self.cmdList.setItemText(2, _translate("CtrlWindow", "调整拟合"))
        self.cmdList.setItemText(3, _translate("CtrlWindow", "重新启动"))
        self.cmdList.setItemText(4, _translate("CtrlWindow", "清除设置"))
        self.cmdList.setItemText(5, _translate("CtrlWindow", "串口打印"))
        self.param1.setPlaceholderText(_translate("CtrlWindow", "参数1"))
        self.param2.setPlaceholderText(_translate("CtrlWindow", "参数2"))
        self.command.setPlaceholderText(_translate("CtrlWindow", "产生语句"))
        self.sendButton.setText(_translate("CtrlWindow", "执行"))


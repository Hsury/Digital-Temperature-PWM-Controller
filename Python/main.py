# -*- coding: utf-8 -*-
from PyQt5 import QtCore, QtWidgets, QtGui
from qwt import QwtPlot, QwtPlotCurve, QwtPlotMarker, QwtText
from qtWindow import Ui_MainWindow
from time import sleep
import numpy as np
import sys
import threading
import requests
import json

talking = False
ping = False

def getData():
    global ipAddr, talking, ping
    global mac, version, time, temp, pwm, border
    while talking:
        try:
            esp8266 = requests.get(ipAddr, timeout = 0.1)
            jsondata = esp8266.json()
            mac = jsondata['mac']
            version = jsondata['version']
            time = jsondata['time']
            temp = jsondata['temp']
            pwm = jsondata['pwm']
            border = jsondata['border']
            ping = True
        except:
            ping = False
        sleep(0.1)

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super(MainWindow, self).__init__()
        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)
        self.statusBar().showMessage('等待连接')
        self.ui.connectButton.clicked.connect(self.connect)
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.wndUpdate)
    
    def wndUpdate(self):
        global ping
        global mac, version, time, temp, pwm, border
        if ping:
            if (float(temp) < border[0]):
                color = '#00CCFF'
                stat = '温度偏低'
            elif (float(temp) > border[1]):
                color = '#FFA500'
                stat = '温度偏高'
            else:
                color = '#06B025'
                stat = '温度正常'
            self.statusBar().showMessage('已连接, 远程时间' + time + ', ' + stat)
            self.ui.tempLCD.display(temp)
            self.ui.pwmLCD.display(pwm)
            self.ui.borderLLCD.display(border[0])
            self.ui.borderHLCD.display(border[1])
            self.ui.tempBar.setStyleSheet('QProgressBar::chunk {background-color: %s;}' % color)
            self.ui.tempBar.setValue(float(temp))
            self.ui.pwmBar.setValue(pwm)
            self.ui.mac.setText('MAC: ' + mac)
            self.ui.version.setText('版本: ' + version)
    
    def connect(self):
        global plotWnd, timerID
        global ipAddr, talking
        talking = not talking
        if talking:
            self.timer.start(100)
            ipAddr = 'http://'\
                     + self.ui.ipSeg1.text() + '.'\
                     + self.ui.ipSeg2.text() + '.'\
                     + self.ui.ipSeg3.text() + '.'\
                     + self.ui.ipSeg4.text()
            try:
                global thd
                thd = threading.Thread(target = getData)
                thd.start()
                plotWnd = PlotWindow()
                plotWnd.setWindowTitle('温度视窗')
                plotWnd.resize(566, 200)
                plotWnd.show()
                plotWnd.setGeometry(plotWnd.geometry().left(), plotWnd.geometry().top() + 240, plotWnd.geometry().width(), plotWnd.geometry().height())
                timerID = plotWnd.startTimer(100)
            except:
                pass
            self.ui.ipSeg1.setEnabled(False)
            self.ui.ipSeg2.setEnabled(False)
            self.ui.ipSeg3.setEnabled(False)
            self.ui.ipSeg4.setEnabled(False)
            self.ui.connectButton.setText('断开')
        else:
            try:
                self.timer.stop()
                plotWnd.killTimer(timerID)
                plotWnd.hide()
            except:
                pass
            self.statusBar().showMessage('等待连接')
            self.ui.tempLCD.display(0)
            self.ui.pwmLCD.display(0)
            self.ui.borderLLCD.display(0)
            self.ui.borderHLCD.display(0)
            self.ui.tempBar.setStyleSheet('QProgressBar::chunk {background-color: #06B025;}')
            self.ui.tempBar.setValue(0)
            self.ui.pwmBar.setValue(0)
            self.ui.mac.setText('MAC: 未知')
            self.ui.version.setText('版本: 未知')
            self.ui.ipSeg1.setEnabled(True)
            self.ui.ipSeg2.setEnabled(True)
            self.ui.ipSeg3.setEnabled(True)
            self.ui.ipSeg4.setEnabled(True)
            self.ui.connectButton.setText('连接')
    
    def closeEvent(self, event):
        global plotWnd
        global talking
        talking = False
        try:
            plotWnd.hide()
        except:
            pass

class PlotWindow(QwtPlot):
    def __init__(self, *args):
        QwtPlot.__init__(self, *args)
        global x, y, lineL, lineH, curveTemp
        HISTORY = 300
        x = 0.1 * np.arange(0, -HISTORY, -1)
        y = np.zeros(HISTORY, np.float)
        self.setAxisScale(QwtPlot.yLeft, 0, 100)
        lineL = QwtPlotMarker()
        lineL.setLinePen(QtGui.QPen(QtCore.Qt.blue))
        lineL.setLabelAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignBottom)
        lineL.setLineStyle(QwtPlotMarker.HLine)
        lineL.setYValue(0)
        lineL.attach(self)
        lineH = QwtPlotMarker()
        lineH.setLinePen(QtGui.QPen(QtCore.Qt.red))
        lineH.setLabelAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignTop)
        lineH.setLineStyle(QwtPlotMarker.HLine)
        lineH.setYValue(100)
        lineH.attach(self)
        curveTemp = QwtPlotCurve("实时温度")
        curveTemp.attach(self)
    
    def timerEvent(self, e):
        global x, y, lineL, lineH, curveTemp
        global ping
        global time, temp, border
        y[1:] = y[0: -1]
        if ping:
            y[0] = float(temp)
            lineL.setYValue(border[0])
            lineH.setYValue(border[1])
        curveTemp.setData(x, y)
        x += 0.1
        self.replot()

def main():
    app = QtWidgets.QApplication(sys.argv)
    mainWnd = MainWindow()
    mainWnd.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()

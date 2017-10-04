from PyQt5 import QtCore, QtWidgets
from qtWindow import Ui_MainWindow
from time import sleep
import sys
import threading
import requests
import json

talking = False

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

class ApplicationWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super(ApplicationWindow, self).__init__()
        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)
        self.ui.connectButton.clicked.connect(self.connect)
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.wndUpdate)
    
    def wndUpdate(self):
        global ping
        if ping:
            self.ui.tempLCD.display(temp)
            self.ui.pwmLCD.display(pwm)
            self.ui.borderLLCD.display(border[0])
            self.ui.borderHLCD.display(border[1])
            self.ui.mac.setText('MAC: ' + mac)
            self.ui.version.setText('版本: ' + version)
    
    def connect(self):
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
            except:
                pass
            self.ui.ipSeg1.setEnabled(False)
            self.ui.ipSeg2.setEnabled(False)
            self.ui.ipSeg3.setEnabled(False)
            self.ui.ipSeg4.setEnabled(False)
            self.ui.connectButton.setText('断开')
        else:
            self.timer.stop()
            self.ui.ipSeg1.setEnabled(True)
            self.ui.ipSeg2.setEnabled(True)
            self.ui.ipSeg3.setEnabled(True)
            self.ui.ipSeg4.setEnabled(True)
            self.ui.connectButton.setText('连接')
    
    def closeEvent(self, event):
        global talking
        talking = False

def main():
    app = QtWidgets.QApplication(sys.argv)
    application = ApplicationWindow()
    application.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()

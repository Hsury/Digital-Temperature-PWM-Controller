from PyQt5 import QtCore, QtWidgets
from qtWindow import Ui_MainWindow
from time import sleep
import sys
import requests
import json

talking = False

class ApplicationWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super(ApplicationWindow, self).__init__()
        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)
        self.ui.connectButton.clicked.connect(self.connectESP8266)
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.wndUpdate)
    
    def wndUpdate(self):
        try:
            esp8266 = requests.get(ipAddr, timeout = 0.25)
            jsondata = esp8266.json()
            mac = jsondata['mac']
            version = jsondata['version']
            time = jsondata['time']
            stat = jsondata['stat']
            sensor = jsondata['sensor']
            bias = jsondata['bias']
            pwm = jsondata['pwm']
            range = jsondata['range']
            ping = True
        except requests.exceptions.ConnectTimeout:
            ping = False
        except requests.exceptions.Timeout:
            ping = False
        except json.decoder.JSONDecodeError:
            ping = False
        if (ping == True):
            self.ui.tempLCD.display(sensor + bias)
            self.ui.pwmLCD.display(pwm)
            self.ui.borderLLCD.display(range[0])
            self.ui.borderHLCD.display(range[1])
            self.ui.mac.setText('MAC: ' + mac)
            self.ui.version.setText('版本: ' + version)
    
    def connectESP8266(self):
        global ipAddr, talking
        talking = not talking
        if (talking == True):
            self.timer.start(500)
            ipAddr = 'http://'\
                     + self.ui.ipSeg1.text() + '.'\
                     + self.ui.ipSeg2.text() + '.'\
                     + self.ui.ipSeg3.text() + '.'\
                     + self.ui.ipSeg4.text()
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
    
def main():
    app = QtWidgets.QApplication(sys.argv)
    application = ApplicationWindow()
    application.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()